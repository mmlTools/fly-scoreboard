#pragma once

#include <QDialog>
#include <QVector>
#include <QString>

#include "fly_score_state.hpp"

class QVBoxLayout;
class QPushButton;
class QLineEdit;
class QSpinBox;
class QCheckBox;

class FlyFieldsDialog : public QDialog {
	Q_OBJECT
public:
	FlyFieldsDialog(const QString &dataDir, FlyState &state, QWidget *parent = nullptr);

private slots:
	void onAddField();
	void onAddSingle();
	void onAccept();

private:
	struct Row {
		QWidget *row = nullptr;
		QLineEdit *labelEdit = nullptr;
		QSpinBox *homeSpin = nullptr;
		QSpinBox *awaySpin = nullptr;
		QPushButton *remove = nullptr;
		bool canRemove = true;
	};

	struct SingleRow {
		QWidget *row = nullptr;
		QLineEdit *labelEdit = nullptr;
		QSpinBox *valueSpin = nullptr;
		QPushButton *remove = nullptr;
		QCheckBox *visible = nullptr;
	};

	void buildUi();
	void loadFromState();
	void saveToState();
	Row addRow(const FlyCustomField &cf, bool canRemove);
	SingleRow addSingleRow(const FlySingleStat &ss);

private:
	QString dataDir_;
	FlyState &st_;

	QVBoxLayout *fieldsLayout_ = nullptr;
	QPushButton *addFieldBtn_ = nullptr;
	QVector<Row> rows_;

	QVBoxLayout *singleLayout_ = nullptr;
	QPushButton *addSingleBtn_ = nullptr;
	QVector<SingleRow> singles_;

	QVector<bool> vis_;
};
