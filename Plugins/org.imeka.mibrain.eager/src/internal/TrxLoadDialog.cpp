#include "TrxLoadDialog.hpp"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStandardItemModel>
#include <QStylePainter>
#include <QVBoxLayout>

using Coloring = mitk::FilteredFiberBundle::Coloring;

CheckableComboBox::CheckableComboBox(QWidget* parent)
  : QComboBox(parent)
  , m_Model(new QStandardItemModel(this))
{
  setModel(m_Model);

  // A click toggles the item instead of selecting it, and keeps the popup
  // open so several groups can be (un)checked in a row.
  connect(view(), &QAbstractItemView::pressed, this,
    [this](const QModelIndex& index)
    {
      auto item = m_Model->itemFromIndex(index);
      item->setCheckState(
        item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
      m_KeepOpen = true;
    });
  connect(m_Model, &QStandardItemModel::itemChanged, this, [this] { update(); });
}

void CheckableComboBox::AddCheckableItem(const QString& text, const QString& name)
{
  auto item = new QStandardItem(text);
  item->setData(name, Qt::UserRole);
  // Not Qt::ItemIsUserCheckable: the check box would be toggled a second
  // time by the item delegate. The check state is still drawn.
  item->setFlags(Qt::ItemIsEnabled);
  item->setCheckState(Qt::Checked); // Everything is loaded by default
  m_Model->appendRow(item);
}

std::set<std::string> CheckableComboBox::CheckedNames() const
{
  std::set<std::string> names;
  for (int row = 0; row < m_Model->rowCount(); ++row)
  {
    const auto item = m_Model->item(row);
    if (item->checkState() == Qt::Checked)
    {
      names.insert(item->data(Qt::UserRole).toString().toStdString());
    }
  }
  return names;
}

int CheckableComboBox::NbChecked() const
{
  int nbChecked = 0;
  for (int row = 0; row < m_Model->rowCount(); ++row)
  {
    nbChecked += m_Model->item(row)->checkState() == Qt::Checked;
  }
  return nbChecked;
}

void CheckableComboBox::hidePopup()
{
  if (m_KeepOpen)
  {
    m_KeepOpen = false;
    return;
  }
  QComboBox::hidePopup();
}

void CheckableComboBox::paintEvent(QPaintEvent*)
{
  // Show a summary instead of the current item
  QStylePainter painter(this);
  QStyleOptionComboBox option;
  initStyleOption(&option);
  option.currentText = QString("%1 / %2 groups selected")
    .arg(NbChecked()).arg(m_Model->rowCount());
  option.currentIcon = QIcon();
  painter.drawComplexControl(QStyle::CC_ComboBox, option);
  painter.drawControl(QStyle::CE_ComboBoxLabel, option);
}

TrxLoadDialog::TrxLoadDialog(const TrxFileInfo& info, QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle("Open TRX tractogram");
  const QLocale locale;

  auto form = new QFormLayout;
  form->addRow("File:", new QLabel(QString::fromStdString(info.filename)));
  form->addRow("Points:",
    new QLabel(locale.toString(static_cast<qulonglong>(info.nbPoints))));
  form->addRow("Streamlines:",
    new QLabel(locale.toString(static_cast<qulonglong>(info.nbStreamlines))));

  // Only the colorings that need nothing else than the streamlines.
  // "From anatomy" needs an image and "Uniform" a color.
  m_Coloring = new QComboBox;
  m_Coloring->addItem("Orientation", static_cast<int>(Coloring::Orientation));
  m_Coloring->addItem("End points", static_cast<int>(Coloring::EndPoint));
  m_Coloring->addItem("Curvature", static_cast<int>(Coloring::Curvature));
  form->addRow("Coloring:", m_Coloring);

  if (info.groups.empty())
  {
    form->addRow("Groups:", new QLabel("No group in this file"));
  }
  else
  {
    m_Groups = new CheckableComboBox;
    for (const auto& nameAndSize : info.groups)
    {
      const auto name = QString::fromStdString(nameAndSize.first);
      m_Groups->AddCheckableItem(
        QString("%1 (%2)").arg(name,
          locale.toString(static_cast<qulonglong>(nameAndSize.second))),
        name);
    }
    connect(m_Groups->model(), &QAbstractItemModel::dataChanged,
      this, [this] { UpdateOkButton(); });
    form->addRow("Groups:", m_Groups);
  }

  m_Buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(m_Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(m_Buttons);
  setMinimumWidth(350);

  UpdateOkButton();
}

void TrxLoadDialog::UpdateOkButton()
{
  // Nothing would be loaded if every group is unchecked
  m_Buttons->button(QDialogButtonBox::Ok)->setEnabled(
    m_Groups == nullptr || m_Groups->NbChecked() > 0);
}

TrxLoadChoice TrxLoadDialog::GetChoice() const
{
  TrxLoadChoice choice;
  choice.coloring = static_cast<Coloring>(m_Coloring->currentData().toInt());

  // All the groups checked: load the whole file. It's faster and it keeps the
  // streamlines that are in no group.
  if (m_Groups && m_Groups->NbChecked() != m_Groups->count())
  {
    choice.groups = m_Groups->CheckedNames();
  }
  return choice;
}
