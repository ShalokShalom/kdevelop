/*
 * Copyright 2014 Kevin Funk <kfunk@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "clangproblem.h"
#include <interfaces/idocumentcontroller.h>
#include <interfaces/icore.h>

#include "util/clangtypes.h"
#include "util/clangdebug.h"

#include <language/duchain/duchainlock.h>
#include <language/codegen/documentchangeset.h>

#include <KLocalizedString>

using namespace KDevelop;

namespace {

IProblem::Severity diagnosticSeverityToSeverity(CXDiagnosticSeverity severity, const QString& optionName)
{
    switch (severity) {
    case CXDiagnostic_Fatal:
    case CXDiagnostic_Error:
        return IProblem::Error;
    case CXDiagnostic_Warning:
        if (optionName.startsWith(QLatin1String("-Wunused-"))) {
            return IProblem::Hint;
        }
        return IProblem::Warning;
    default:
        return IProblem::Hint;
    }
}

/**
 * Clang diagnostic messages always start with a lowercase character
 *
 * @return Prettified version, starting with uppercase character
 */
inline QString prettyDiagnosticSpelling(const QString& str)
{
    QString ret = str;
    if (ret.isEmpty()) {
      return {};
    }
    ret[0] = ret[0].toUpper();
    return ret;
}

ClangFixits fixitsForDiagnostic(CXDiagnostic diagnostic)
{
    ClangFixits fixits;
    auto numFixits = clang_getDiagnosticNumFixIts(diagnostic);
    fixits.reserve(numFixits);
    for (uint i = 0; i < numFixits; ++i) {
        CXSourceRange range;
        const QString replacementText = ClangString(clang_getDiagnosticFixIt(diagnostic, i, &range)).toString();

        const auto docRange = ClangRange(range).toDocumentRange();
        auto doc = KDevelop::ICore::self()->documentController()->documentForUrl(docRange.document.toUrl());
        const QString original = doc ? doc->text(docRange) : QString{};

        fixits << ClangFixit{replacementText, docRange, QString(), original};
    }
    return fixits;
}

}

QDebug operator<<(QDebug debug, const ClangFixit& fixit)
{
    debug.nospace() << "ClangFixit["
        << "replacementText=" << fixit.replacementText
        << ", range=" << fixit.range
        << ", description=" << fixit.description
        << "]";
    return debug;
}

ClangProblem::ClangProblem() = default;

ClangProblem::ClangProblem(const ClangProblem& other)
    : Problem(),
      m_fixits(other.m_fixits)
{
    setSource(other.source());
    setFinalLocation(other.finalLocation());
    setFinalLocationMode(other.finalLocationMode());
    setDescription(other.description());
    setExplanation(other.explanation());
    setSeverity(other.severity());

    auto diagnostics = other.diagnostics();
    for (auto& diagnostic : diagnostics) {
        auto* clangDiagnostic = dynamic_cast<ClangProblem*>(diagnostic.data());
        Q_ASSERT(clangDiagnostic);
        diagnostic = ClangProblem::Ptr(new ClangProblem(*clangDiagnostic));
    }
    setDiagnostics(diagnostics);
}

ClangProblem::ClangProblem(CXDiagnostic diagnostic, CXTranslationUnit unit)
{
    const QString diagnosticOption = ClangString(clang_getDiagnosticOption(diagnostic, nullptr)).toString();

    auto severity = diagnosticSeverityToSeverity(clang_getDiagnosticSeverity(diagnostic), diagnosticOption);
    setSeverity(severity);

    QString description = ClangString(clang_getDiagnosticSpelling(diagnostic)).toString();
    if (!diagnosticOption.isEmpty()) {
        description.append(QLatin1String(" [") + diagnosticOption + QLatin1Char(']'));
    }
    setDescription(prettyDiagnosticSpelling(description));

    ClangLocation location(clang_getDiagnosticLocation(diagnostic));
    CXFile diagnosticFile;
    clang_getFileLocation(location, &diagnosticFile, nullptr, nullptr, nullptr);
    const ClangString fileName(clang_getFileName(diagnosticFile));
    DocumentRange docRange(IndexedString(QUrl::fromLocalFile(fileName.toString()).adjusted(QUrl::NormalizePathSegments)), KTextEditor::Range(location, location));
    const uint numRanges = clang_getDiagnosticNumRanges(diagnostic);
    for (uint i = 0; i < numRanges; ++i) {
        auto range = ClangRange(clang_getDiagnosticRange(diagnostic, i)).toRange();
        if(!range.isValid()){
            continue;
        }

        if (range.start() < docRange.start()) {
            docRange.setStart(range.start());
        }
        if (range.end() > docRange.end()) {
            docRange.setEnd(range.end());
        }
    }
    if (docRange.isEmpty()) {
        // try to find a bigger range for the given location by using the token at the given location
        CXFile file = nullptr;
        unsigned line = 0;
        unsigned column = 0;
        clang_getExpansionLocation(location, &file, &line, &column, nullptr);
        // just skip ahead some characters, hoping that it's sufficient to encompass
        // a token we can use for building the range
        auto nextLocation = clang_getLocation(unit, file, line, column + 100);
        auto rangeToTokenize = clang_getRange(location, nextLocation);
        const ClangTokens tokens(unit, rangeToTokenize);
        if (tokens.size()) {
            docRange.setRange(ClangRange(clang_getTokenExtent(unit, tokens.at(0))).toRange());
        }
    }

    setFixits(fixitsForDiagnostic(diagnostic));
    setFinalLocation(docRange);
    setSource(IProblem::SemanticAnalysis);

    QVector<IProblem::Ptr> diagnostics;
    auto childDiagnostics = clang_getChildDiagnostics(diagnostic);
    auto numChildDiagnostics = clang_getNumDiagnosticsInSet(childDiagnostics);
    diagnostics.reserve(numChildDiagnostics);
    for (uint j = 0; j < numChildDiagnostics; ++j) {
        auto childDiagnostic = clang_getDiagnosticInSet(childDiagnostics, j);
        ClangProblem::Ptr problem(new ClangProblem(childDiagnostic, unit));
        diagnostics << ProblemPointer(problem.data());
    }
    setDiagnostics(diagnostics);
}

IAssistant::Ptr ClangProblem::solutionAssistant() const
{
    if (allFixits().isEmpty()) {
        return {};
    }

    return IAssistant::Ptr(new ClangFixitAssistant(allFixits()));
}

ClangFixits ClangProblem::fixits() const
{
    return m_fixits;
}

void ClangProblem::setFixits(const ClangFixits& fixits)
{
    m_fixits = fixits;
}

ClangFixits ClangProblem::allFixits() const
{
    ClangFixits result;
    result << m_fixits;

    const auto& diagnostics = this->diagnostics();
    for (const IProblem::Ptr& diagnostic : diagnostics) {
        const Ptr problem(dynamic_cast<ClangProblem*>(diagnostic.data()));
        Q_ASSERT(problem);
        result << problem->allFixits();
    }
    return result;
}

ClangFixitAssistant::ClangFixitAssistant(const ClangFixits& fixits)
    : m_title(i18n("Fix-it Hints"))
    , m_fixits(fixits)
{
}

ClangFixitAssistant::ClangFixitAssistant(const QString& title, const ClangFixits& fixits)
    : m_title(title)
    , m_fixits(fixits)
{
}

QString ClangFixitAssistant::title() const
{
    return m_title;
}

void ClangFixitAssistant::createActions()
{
    KDevelop::IAssistant::createActions();

    for (const ClangFixit& fixit : qAsConst(m_fixits)) {
        addAction(IAssistantAction::Ptr(new ClangFixitAction(fixit)));
    }
}

ClangFixits ClangFixitAssistant::fixits() const
{
    return m_fixits;
}

ClangFixitAction::ClangFixitAction(const ClangFixit& fixit)
    : m_fixit(fixit)
{
}

QString ClangFixitAction::description() const
{
    if (!m_fixit.description.isEmpty())
        return m_fixit.description;

    const auto range = m_fixit.range;
    if (range.start() == range.end()) {
        return i18n("Insert \"%1\" at line: %2, column: %3",
                    m_fixit.replacementText, range.start().line()+1, range.start().column()+1);
    } else if (range.start().line() == range.end().line()) {
        if (m_fixit.currentText.isEmpty()) {
            return i18n("Replace text at line: %1, column: %2 with: \"%3\"",
                    range.start().line()+1, range.start().column()+1, m_fixit.replacementText);
        } else
            return i18n("Replace \"%1\" with: \"%2\"",
                    m_fixit.currentText, m_fixit.replacementText);
    } else {
        return i18n("Replace multiple lines starting at line: %1, column: %2 with: \"%3\"",
                    range.start().line()+1, range.start().column()+1, m_fixit.replacementText);
    }
}

void ClangFixitAction::execute()
{
    DocumentChangeSet changes;
    {
        DUChainReadLocker lock;

        DocumentChange change(m_fixit.range.document, m_fixit.range,
                    m_fixit.currentText, m_fixit.replacementText);
        change.m_ignoreOldText = !m_fixit.currentText.isEmpty();
        changes.addChange(change);
    }

    changes.setReplacementPolicy(DocumentChangeSet::WarnOnFailedChange);
    changes.applyAllChanges();
    emit executed(this);
}
