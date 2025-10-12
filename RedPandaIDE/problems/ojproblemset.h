/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef OJPROBLEMSET_H
#define OJPROBLEMSET_H
#include <QString>
#include <memory>
#include <QVector>
#include <QList>
#include <QObject>
enum class ProblemCaseTestState {
    NotTested,
    Testing,
    Passed,
    Failed
};

enum class ProblemTimeLimitUnit {
    Seconds,
    Milliseconds
};

enum class ProblemMemoryLimitUnit {
    KB,
    MB,
    GB
};

class  OJProblemCase : public QObject{
    Q_OBJECT
public:
    explicit OJProblemCase(QObject* parent = nullptr);
    OJProblemCase(const OJProblemCase &) = delete;
    const QString &id() const;

    const QString &name() const;
    void setName(const QString &newName);
    const QString &input() const;
    void setInput(const QString &newInput);
    const QString &expected() const;
    void setExpected(const QString &newExpected);
    const QString &inputFileName() const;
    void setInputFileName(const QString &newInputFileName);
    const QString &expectedOutputFileName() const;
    void setExpectedOutputFileName(const QString &newExpectedOutputFileName);

    bool isModified();
    void setModified(bool newModified);
signals:
    void modifiedChanged(const QString& id);

private:
    QString mName;
    QString mInput;
    QString mExpected;
    QString mInputFileName;
    QString mExpectedOutputFileName;
    QString mId;
    bool mModified;
public:
    ProblemCaseTestState testState; // no persistence
    QString output; // no persistence
    qulonglong runningTime; // no persistence
    qulonglong runningMemory; // no persistence;
    int outputLineCounts; // no persistence;
    int expectedLineCounts; // no persistence;
    int firstDiffLine; // no persistence
};

using POJProblemCase = std::shared_ptr<OJProblemCase>;

class OJProblem : public QObject {
    Q_OBJECT
public:
    explicit OJProblem(QObject *parent = nullptr);
    OJProblem(const OJProblem &) = delete;
    const QString &id() const;
    bool isModified() const;
    void setModified(bool newModified);

    const QString &name() const;
    void setName(const QString &newName);
    const QString &url() const;
    void setUrl(const QString &newUrl);
    const QString &description() const;
    void setDescription(const QString &newDescription);
    const QString &hint() const;
    void setHint(const QString &newHint);
    const QString &answerProgram() const;
    void setAnswerProgram(const QString &newAnswerProgram);
    void setTimeLimit(size_t newTimeLimit);
    void setMemoryLimit(size_t newMemoryLimit);
    size_t getTimeLimitInMilliseconds();
    size_t getMemoryLimitInBytes();
    ProblemTimeLimitUnit timeLimitUnit() const;
    void setTimeLimitUnit(ProblemTimeLimitUnit newTimeLimitUnit);
    ProblemMemoryLimitUnit memoryLimitUnit() const;
    void setMemoryLimitUnit(ProblemMemoryLimitUnit newMemoryLimitUnit);
    const QVector<POJProblemCase> &cases() const;
    void addCase(POJProblemCase& problemCase);
    void removeCase(int idx);
    void moveCase(int fromIdx, int toIdx);
    void clearCases();
    size_t timeLimit() const;

    size_t memoryLimit() const;

signals:
    void problemCaseModified(const QString& caseId);
    void modifiedChanged(const QString& problemId);

private slots:
    void onProblemCaseModified(const QString& caseId);
private:
    QString mId;
    QString mName;
    QString mUrl;
    QString mDescription;
    QString mHint;
    QString mAnswerProgram;
    size_t mTimeLimit;
    size_t mMemoryLimit;
    ProblemTimeLimitUnit mTimeLimitUnit;
    ProblemMemoryLimitUnit mMemoryLimitUnit;
    QVector<POJProblemCase> mCases;
    bool mModified;
};

using POJProblem = std::shared_ptr<OJProblem>;

class OJProblemSet : public  QObject{
    Q_OBJECT
public:
    explicit OJProblemSet(QObject *parent = nullptr);
    OJProblemSet(const OJProblemSet &) = delete;
    bool isModified() const;
    void setModified(bool newModified);

    const QString &name() const;
    void setName(const QString &newName);

    const QList<POJProblem> &problems() const;

    const QString &exportFilename() const;
    void setExportFilename(const QString &newExportFilename);

    void addProblem(const POJProblem &problem);
    void removeProblem(int idx);
    void clearProblems();
    void moveProblem(int fromIdx, int toIdx);

signals:
    void problemModified(const QString &id);
    void modifiedChanged();
private slots:
    void onProblemModified(const QString& id);
private:
    QString mName;
    QList<POJProblem> mProblems;
    QString mExportFilename;
    bool mModified;
};

using POJProblemSet  = std::shared_ptr<OJProblemSet>;

#endif // OJPROBLEMSET_H
