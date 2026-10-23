#pragma once
#include <QMainWindow>

class Packager;
struct Project;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class SidePanel;

class BuilderWindow : public QMainWindow {
    Q_OBJECT
public:
    BuilderWindow();
    bool openProject(const QString &path);          // charge un fichier .kit-gen dans le formulaire

private slots:
    void pickExe();
    void pickIcon();
    void pickLicense();
    void pickOutput();
    void updatePreview();
    void refreshGpgKeys();
    void updateGpgUi();
    void newProject();
    void openDialog();
    bool saveProject();
    bool saveProjectAs();
    void build();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    Project currentProject() const;
    void applyProject(const Project &p, const QString &path);
    bool doSave(const QString &path);
    void markDirty();
    void updateTitle();
    bool confirmDiscard(const QString &title);
    QWidget *rowWithButton(QLineEdit *edit, const QString &btnText, const char *slot);
    void log(const QString &line);
    void setBusy(bool busy);
    void onDone(bool ok, const QString &file, const QString &error);

    QLineEdit *m_exe, *m_icon, *m_name, *m_version, *m_dev, *m_email, *m_site, *m_licFile, *m_out, *m_progLangs;
    QComboBox *m_license, *m_category, *m_gpgKey;
    QCheckBox *m_sign;
    QLabel *m_gpgHint;
    QPlainTextEdit *m_desc, *m_log;
    QPushButton *m_buildBtn;
    QProgressBar *m_bar;
    SidePanel *m_panel;
    QLabel *m_projLabel;
    QString m_projectPath;
    bool m_dirty = false, m_loading = false;
};
