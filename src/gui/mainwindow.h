#pragma once

#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
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
    void verify();

private:
    QLineEdit *m_input = nullptr;
    QLineEdit *m_output = nullptr;
    QComboBox *m_encoding = nullptr;
    QCheckBox *m_compact = nullptr;
    QCheckBox *m_lossless = nullptr;
    QPushButton *m_convert = nullptr;
    QTextEdit *m_log = nullptr;
};

} // namespace arena::tes3json
