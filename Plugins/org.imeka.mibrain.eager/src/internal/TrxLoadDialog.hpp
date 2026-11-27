#ifndef IMEKA_MIBRAIN_EAGER_TRX_LOAD_DIALOG_HPP_INCLUDED
#define IMEKA_MIBRAIN_EAGER_TRX_LOAD_DIALOG_HPP_INCLUDED

#include <set>
#include <string>

#include <QComboBox>
#include <QDialog>

#include "FiberBundle/Trx.h"

class QCheckBox;
class QDialogButtonBox;
class QStandardItemModel;

// Combo box whose items are check boxes. The popup stays open while items are
// checked, and the box shows how many items are checked.
class CheckableComboBox : public QComboBox
{
public:
  explicit CheckableComboBox(QWidget* parent = nullptr);

  // `text` is displayed, `name` is returned by CheckedNames()
  void AddCheckableItem(const QString& text, const QString& name);
  std::set<std::string> CheckedNames() const;
  int NbChecked() const;

  void hidePopup() override;

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QStandardItemModel* m_Model;
  bool m_KeepOpen = false;
};

// Shown before a .trx file is loaded: file name, number of points and of
// streamlines, the coloring to apply, the groups to load and whether the
// streamlines of the other groups are kept (loaded but hidden) or lost.
class TrxLoadDialog : public QDialog
{
public:
  explicit TrxLoadDialog(const TrxFileInfo&, QWidget* parent = nullptr);

  TrxLoadChoice GetChoice() const;

private:
  void UpdateButtons();

  QComboBox* m_Coloring;
  CheckableComboBox* m_Groups = nullptr; // nullptr if the file has no group
  QCheckBox* m_KeepOthers = nullptr;     // nullptr if the file has no group
  QDialogButtonBox* m_Buttons;
};

#endif // IMEKA_MIBRAIN_EAGER_TRX_LOAD_DIALOG_HPP_INCLUDED
