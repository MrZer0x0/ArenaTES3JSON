#pragma once

#include <QMainWindow>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTextEdit;

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
    void verify();

private:
    QLineEdit *m_input = nullptr;
    QLineEdit *m_output = nullptr;
    QCheckBox *m_compact = nullptr;
    QPushButton *m_convert = nullptr;
    QTextEdit *m_log = nullptr;
};

} // namespace arena::tes3json
