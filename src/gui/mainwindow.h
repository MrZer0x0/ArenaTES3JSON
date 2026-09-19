#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QProcess>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;

namespace arena::tes3json {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void browseInput();
    void browseOutput();
    void updateOutput();
    void convert();
    void readBackendStdout();
    void readBackendStderr();
    void backendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void backendError(QProcess::ProcessError error);

private:
    void setBusy(bool busy);
    void handleProgressLine(const QByteArray &line);
    QString directionText() const;

    QLineEdit *m_input = nullptr;
    QLineEdit *m_output = nullptr;
    QLabel *m_direction = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_inputBrowse = nullptr;
    QPushButton *m_outputBrowse = nullptr;
    QPushButton *m_convert = nullptr;
    QProcess *m_process = nullptr;

    QByteArray m_stdout;
    QByteArray m_stderrPending;
    QByteArray m_errorText;
};

} // namespace arena::tes3json
