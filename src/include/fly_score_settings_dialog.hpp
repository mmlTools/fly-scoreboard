#pragma once
#include <QDialog>
#include <QString>

class QLabel;
class QSpinBox;
class QPushButton;
class QTimer;
class QLineEdit;

class FlySettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit FlySettingsDialog(QWidget *parent = nullptr);
    ~FlySettingsDialog() override;

private slots:
    void onPollHealth();
    void onRestartServer();
    void onOpenOverlay();
    void onOpenHotkeysHelp();
    void onBrowseDocRoot();

private:
    void setStatusUi(bool ok, int port);
    bool healthOkHttp(int port, int timeout_ms = 400);

private:
    QLabel *statusDot_          = nullptr;
    QLabel *statusText_         = nullptr;
    QSpinBox *portSpin_         = nullptr;
    QLineEdit *docRootEdit_     = nullptr;
    QPushButton *restartBtn_    = nullptr;
    QPushButton *openFolderBtn_ = nullptr;
    QPushButton *hotkeysBtn_    = nullptr;
    QPushButton *browseDocRootBtn_ = nullptr;
    QTimer *statusTimer_        = nullptr;
};
