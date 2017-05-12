/**
 * @file  DialogPreferences.cpp
 * @brief REPLACE_WITH_ONE_LINE_SHORT_DESCRIPTION
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2017/02/02 18:41:17 $
 *    $Revision: 1.23 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */
#include "DialogPreferences.h"
#include "ui_DialogPreferences.h"
#include "MainWindow.h"
#include "RenderView2D.h"
#include "RenderView3D.h"
#include "Cursor2D.h"
#include "Cursor3D.h"
#include "Annotation2D.h"
#include "TermWidget.h"

DialogPreferences::DialogPreferences(QWidget *parent) :
  QDialog(parent),
  UIUpdateHelper(),
  ui(new Ui::DialogPreferences)
{
  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(true);
  MainWindow* mainwnd = MainWindow::GetMainWindow();
  for (int i = 0; i < 4; i++)
  {
    connect(ui->colorPickerBackground, SIGNAL(colorChanged(QColor)),
            mainwnd->GetRenderView(i), SLOT(SetBackgroundColor(QColor)));
  }
  for (int i = 0; i < 3; i++)
  {
    connect(ui->colorPickerCursor, SIGNAL(colorChanged(QColor)),
            ((RenderView2D*)mainwnd->GetRenderView(i))->GetCursor2D(), SLOT(SetColor(QColor)));
    connect(ui->horizontalSliderSize2D, SIGNAL(valueChanged(int)),
            ((RenderView2D*)mainwnd->GetRenderView(i))->GetCursor2D(), SLOT(SetSize(int)));
    connect(ui->horizontalSliderThickness2D, SIGNAL(valueChanged(int)),
            ((RenderView2D*)mainwnd->GetRenderView(i))->GetCursor2D(), SLOT(SetThickness(int)));
    connect(ui->colorPickerAnnotation, SIGNAL(colorChanged(QColor)),
            ((RenderView2D*)mainwnd->GetRenderView(i))->GetAnnotation2D(), SLOT(SetColor(QColor)));
  }
  connect(ui->colorPickerCursor, SIGNAL(colorChanged(QColor)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetCursor3D(), SLOT(SetColor(QColor)));
  connect(ui->colorPickerCursor, SIGNAL(colorChanged(QColor)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetInflatedSurfCursor(), SLOT(SetColor(QColor)));
  connect(ui->horizontalSliderSize3D, SIGNAL(valueChanged(int)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetCursor3D(), SLOT(SetSize(int)));
  connect(ui->horizontalSliderSize3D, SIGNAL(valueChanged(int)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetInflatedSurfCursor(), SLOT(SetSize(int)));
  connect(ui->horizontalSliderThickness3D, SIGNAL(valueChanged(int)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetCursor3D(), SLOT(SetThickness(int)));
  connect(ui->horizontalSliderThickness3D, SIGNAL(valueChanged(int)),
          ((RenderView3D*)mainwnd->GetRenderView(3))->GetInflatedSurfCursor(), SLOT(SetThickness(int)));
  connect(ui->checkBoxSyncZoom, SIGNAL(toggled(bool)),
          mainwnd, SLOT(SyncZoom(bool)));
  connect(ui->radioButtonThemeDark, SIGNAL(toggled(bool)),
          mainwnd->GetCommandConsole(), SLOT(SetDarkTheme(bool)));

#ifdef Q_OS_MAC
  ui->groupBoxMac->setEnabled(true);
  ui->groupBoxMac->show();
  connect(ui->checkBoxMacUnified, SIGNAL(toggled(bool)), mainwnd, SLOT(SetUnifiedTitleAndToolBar(bool)));
  connect(ui->checkBoxCommandKey, SIGNAL(toggled(bool)), mainwnd, SLOT(SetUseCommandControl(bool)));
#else
  ui->groupBoxMac->setEnabled(false);
  ui->groupBoxMac->hide();
#endif

  connect(ui->colorPickerAnnotation, SIGNAL(colorChanged(QColor)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->colorPickerBackground, SIGNAL(colorChanged(QColor)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->colorPickerCursor, SIGNAL(colorChanged(QColor)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->horizontalSliderSize2D, SIGNAL(valueChanged(int)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->horizontalSliderSize3D, SIGNAL(valueChanged(int)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->checkBoxRightButtonErase, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->checkBoxSaveCopy, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->checkBoxSyncZoom, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->radioButtonThemeDark, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->checkBoxAutoReorientView, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
  connect(ui->checkBoxDecimalVoxelCoord, SIGNAL(toggled(bool)), mainwnd, SLOT(UpdateSettings()));
}

DialogPreferences::~DialogPreferences()
{
  delete ui;
}

void DialogPreferences::SetSettings(const QVariantMap &map)
{
  BlockAllSignals(this, true);
  ui->colorPickerBackground->setCurrentColor(map["BackgroundColor"].value<QColor>());
  ui->colorPickerCursor->setCurrentColor(map["CursorColor"].value<QColor>());
  ui->horizontalSliderSize2D->setValue(map["CursorSize"].toInt());
  ui->horizontalSliderSize3D->setValue(map["CursorSize3D"].toInt());
  ui->horizontalSliderThickness2D->setValue(map["CursorThickness"].toInt());
  ui->horizontalSliderThickness3D->setValue(map["CursorThickness3D"].toInt());
  ui->checkBoxSaveCopy->setChecked(map["SaveCopy"].toBool());
  ui->checkBoxSyncZoom->setChecked(map["SyncZoom"].toBool());
  ui->checkBoxCommandKey->setChecked(map["MacUseCommand"].toBool());
  ui->checkBoxMacUnified->setChecked(map["MacUnifiedTitleBar"].toBool());
  ui->radioButtonThemeDark->setChecked(map["DarkConsole"].toBool());
  ui->radioButtonThemeLight->setChecked(!map["DarkConsole"].toBool());
  ui->colorPickerAnnotation->setCurrentColor(map["AnnotationColor"].value<QColor>());
  ui->checkBoxRightButtonErase->setChecked(map["RightButtonErase"].toBool());
  ui->checkBoxAutoReorientView->setChecked(map["AutoReorientView"].toBool());
  ui->checkBoxDecimalVoxelCoord->setChecked(map["DecimalVoxelCoord"].toBool());
  BlockAllSignals(this, false);
}

QVariantMap DialogPreferences::GetSettings()
{
  QVariantMap map;
  map["BackgroundColor"] = ui->colorPickerBackground->currentColor();
  map["CursorColor"] = ui->colorPickerCursor->currentColor();
  map["CursorSize"] = ui->horizontalSliderSize2D->value();
  map["CursorSize3D"] = ui->horizontalSliderSize3D->value();
  map["CursorThickness"] = ui->horizontalSliderThickness2D->value();
  map["CursorThickness3D"] = ui->horizontalSliderThickness3D->value();
  map["SaveCopy"] = ui->checkBoxSaveCopy->isChecked();
  map["SyncZoom"] = ui->checkBoxSyncZoom->isChecked();
  map["MacUseCommand"] = ui->checkBoxCommandKey->isChecked();
  map["MacUnifiedTitleBar"] = ui->checkBoxMacUnified->isChecked();
  map["DarkConsole"] = ui->radioButtonThemeDark->isChecked();
  map["AnnotationColor"] = ui->colorPickerAnnotation->currentColor();
  map["RightButtonErase"] = ui->checkBoxRightButtonErase->isChecked();
  map["AutoReorientView"] = ui->checkBoxAutoReorientView->isChecked();
  map["DecimalVoxelCoord"] = ui->checkBoxDecimalVoxelCoord->isChecked();
  return map;
}

void DialogPreferences::OnClicked(QAbstractButton* btn)
{
  if (ui->buttonBox->buttonRole(btn) == QDialogButtonBox::ResetRole)
  {
    ui->colorPickerBackground->setCurrentColor(Qt::black);
    ui->colorPickerCursor->setCurrentColor(Qt::red);
    ui->colorPickerAnnotation->setCurrentColor(Qt::white);
    ui->horizontalSliderSize2D->setValue(5);
    ui->horizontalSliderSize3D->setValue(1);
    ui->horizontalSliderThickness2D->setValue(1);
    ui->horizontalSliderThickness3D->setValue(1);
    ui->checkBoxSaveCopy->setChecked(true);
    ui->checkBoxRightButtonErase->setChecked(false);
    ui->checkBoxSyncZoom->setChecked(true);
    ui->radioButtonThemeDark->setChecked(true);
#ifdef Q_OS_MAC
    ui->checkBoxCommandKey->setChecked(false);
    ui->checkBoxMacUnified->setChecked(false);
#endif
  }
}
