/***************************************************************************
 *   Copyright (c) 2011 Juergen Riegel <FreeCAD@juergen-riegel.net>        *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>
# include <QRegExp>
# include <QTextStream>
# include <Precision.hxx>
#endif

#include "ui_TaskPadParameters.h"
#include "TaskPadParameters.h"
#include <App/Application.h>
#include <App/Document.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/BitmapFactory.h>
#include <Gui/ViewProvider.h>
#include <Gui/WaitCursor.h>
#include <Base/Console.h>
#include <Gui/Selection.h>
#include <Gui/Command.h>
#include <Mod/PartDesign/App/FeaturePad.h>
#include <Mod/Sketcher/App/SketchObject.h>
#include "TaskSketchBasedParameters.h"
#include "ReferenceSelection.h"

using namespace PartDesignGui;
using namespace Gui;

/* TRANSLATOR PartDesignGui::TaskPadParameters */

TaskPadParameters::TaskPadParameters(ViewProviderPad *PadView, QWidget *parent, bool newObj)
    : TaskSketchBasedParameters(PadView, parent, "PartDesign_Pad",tr("Pad parameters"))
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui = new Ui_TaskPadParameters();
    ui->setupUi(proxy);

    this->groupLayout()->addWidget(proxy);

    // set the history path
    ui->lengthEdit->setParamGrpPath(QByteArray("User parameter:BaseApp/History/PadLength"));
    ui->lengthEdit2->setParamGrpPath(QByteArray("User parameter:BaseApp/History/PadLength2"));
    ui->offsetEdit->setParamGrpPath(QByteArray("User parameter:BaseApp/History/PadOffset"));

    // Get the feature data
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    Base::Quantity l = pcPad->Length.getQuantityValue();
    Base::Quantity l2 = pcPad->Length2.getQuantityValue();
    Base::Quantity off = pcPad->Offset.getQuantityValue();
    bool midplane = pcPad->Midplane.getValue();
    bool reversed = pcPad->Reversed.getValue();
    int index = pcPad->Type.getValue(); // must extract value here, clear() kills it!
    App::DocumentObject* obj = pcPad->UpToFace.getValue();
    std::vector<std::string> subStrings = pcPad->UpToFace.getSubValues();
    std::string upToFace;
    int faceId = -1;
    if ((obj != NULL) && !subStrings.empty()) {
        upToFace = subStrings.front();
        if (upToFace.substr(0,4) == "Face")
            faceId = std::atoi(&upToFace[4]);
    }

    // Fill data into dialog elements
    ui->lengthEdit->setValue(l);
    ui->lengthEdit2->setValue(l2);
    ui->offsetEdit->setValue(off);

    // Bind input fields to properties
    ui->lengthEdit->bind(pcPad->Length);
    ui->lengthEdit2->bind(pcPad->Length2);
    ui->checkBoxMidplane->setChecked(midplane);
    // According to bug #0000521 the reversed option
    // shouldn't be de-activated if the pad has a support face
    ui->checkBoxReversed->setChecked(reversed);
    if ((obj != NULL) && PartDesign::Feature::isDatum(obj))
        ui->lineFaceName->setText(QString::fromLatin1(obj->getNameInDocument()));
    else if (faceId >= 0)
        ui->lineFaceName->setText(QString::fromLatin1(obj->getNameInDocument()) + QString::fromLatin1(":") + tr("Face") +
                                  QString::number(faceId));
    else
        ui->lineFaceName->setText(tr("No face selected"));
    ui->lineFaceName->setProperty("FaceName", QByteArray(upToFace.c_str()));
    ui->changeMode->clear();
    ui->changeMode->insertItem(0, tr("Dimension"));
    ui->changeMode->insertItem(1, tr("To last"));
    ui->changeMode->insertItem(2, tr("To first"));
    ui->changeMode->insertItem(3, tr("Up to face"));
    ui->changeMode->insertItem(4, tr("Two dimensions"));
    ui->changeMode->setCurrentIndex(index);

    QMetaObject::connectSlotsByName(this);

    connect(ui->lengthEdit, SIGNAL(valueChanged(double)),
            this, SLOT(onLengthChanged(double)));
    connect(ui->lengthEdit2, SIGNAL(valueChanged(double)),
            this, SLOT(onLength2Changed(double)));
    connect(ui->offsetEdit, SIGNAL(valueChanged(double)),
            this, SLOT(onOffsetChanged(double)));
    connect(ui->checkBoxMidplane, SIGNAL(toggled(bool)),
            this, SLOT(onMidplaneChanged(bool)));
    connect(ui->checkBoxReversed, SIGNAL(toggled(bool)),
            this, SLOT(onReversedChanged(bool)));
    connect(ui->changeMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onModeChanged(int)));
    connect(ui->buttonFace, SIGNAL(clicked()),
            this, SLOT(onButtonFace()));
    connect(ui->lineFaceName, SIGNAL(textEdited(QString)),
            this, SLOT(onFaceName(QString)));
    connect(ui->checkBoxUpdateView, SIGNAL(toggled(bool)),
            this, SLOT(onUpdateView(bool)));

    // Due to signals attached after changes took took into effect we should update the UI now.
    updateUI(index);

    // if it is a newly created object use the last value of the history
    // TODO: newObj doesn't supplied normally by any caller (2015-07-24, Fat-Zer)
    if(newObj){
        ui->lengthEdit->setToLastUsedValue();
        ui->lengthEdit->selectNumber();
        ui->lengthEdit2->setToLastUsedValue();
        ui->lengthEdit2->selectNumber();
        ui->offsetEdit->setToLastUsedValue();
        ui->offsetEdit->selectNumber();
    }
}

void TaskPadParameters::updateUI(int index)
{
    // disable/hide everything unless we are sure we don't need it
    bool isLengthEditVisable  = false;
    bool isLengthEdit2Visable = false;
    bool isOffsetEditVisable  = false;
    bool isMidplateEnabled    = false;
    bool isReversedEnabled    = false;
    bool isFaceEditEnabled    = false;

    if (index == 0) {  // dimension
        isLengthEditVisable = true;
        ui->lengthEdit->selectNumber();
        // Make sure that the spin box has the focus to get key events
        // Calling setFocus() directly doesn't work because the spin box is not
        // yet visible.
        QMetaObject::invokeMethod(ui->lengthEdit, "setFocus", Qt::QueuedConnection);
        isMidplateEnabled = true;
        // Reverse only makes sense if Midplane is not true
        isReversedEnabled = !ui->checkBoxMidplane->isChecked();
    } else if (index == 1 || index == 2) { // up to first/last
        isOffsetEditVisable  = true;
        isReversedEnabled = true;
    } else if (index == 3) { // up to face
        isOffsetEditVisable  = true;
        isFaceEditEnabled    = true;
        QMetaObject::invokeMethod(ui->lineFaceName, "setFocus", Qt::QueuedConnection);
        // Go into reference selection mode if no face has been selected yet
        if (ui->lineFaceName->text().isEmpty() || (ui->lineFaceName->text() == tr("No face selected")))
            onButtonFace(true);
    } else { // two dimensions
        isLengthEditVisable = true;
        isLengthEdit2Visable = true;
    }

    ui->lengthEdit->setVisible( isLengthEditVisable );
    ui->lengthEdit->setEnabled( isLengthEditVisable );
    ui->labelLength->setVisible( isLengthEditVisable );

    ui->offsetEdit->setVisible( isOffsetEditVisable );
    ui->offsetEdit->setEnabled( isOffsetEditVisable );
    ui->labelOffset->setVisible( isOffsetEditVisable );

    ui->checkBoxMidplane->setEnabled( isMidplateEnabled );

    ui->checkBoxReversed->setEnabled( isReversedEnabled );

    ui->lengthEdit2->setVisible( isLengthEdit2Visable );
    ui->lengthEdit2->setEnabled( isLengthEdit2Visable );
    ui->labelLength2->setVisible( isLengthEdit2Visable );

    ui->buttonFace->setEnabled( isFaceEditEnabled );
    ui->lineFaceName->setEnabled( isFaceEditEnabled );
    if (!isFaceEditEnabled) {
        onButtonFace(false);
    }
}

void TaskPadParameters::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        QString refText = onAddSelection(msg);
        if (refText.length() > 0) {
            ui->lineFaceName->blockSignals(true);
            ui->lineFaceName->setText(refText);
            ui->lineFaceName->setProperty("FaceName", QByteArray(msg.pSubName));
            ui->lineFaceName->blockSignals(false);
            // Turn off reference selection mode
            onButtonFace(false);
        } else {
            ui->lineFaceName->blockSignals(true);
            ui->lineFaceName->setText(tr("No face selected"));
            ui->lineFaceName->setProperty("FaceName", QByteArray());
            ui->lineFaceName->blockSignals(false);
        }
    } else if (msg.Type == Gui::SelectionChanges::ClrSelection) {
        ui->lineFaceName->blockSignals(true);
        ui->lineFaceName->setText(tr("No face selected"));
        ui->lineFaceName->setProperty("FaceName", QByteArray());
        ui->lineFaceName->blockSignals(false);
    }
}

void TaskPadParameters::onLengthChanged(double len)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    pcPad->Length.setValue(len);
    recomputeFeature();
}

void TaskPadParameters::onLength2Changed(double len)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    pcPad->Length2.setValue(len);
    recomputeFeature();
}

void TaskPadParameters::onOffsetChanged(double len)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    pcPad->Offset.setValue(len);
    recomputeFeature();
}

void TaskPadParameters::onMidplaneChanged(bool on)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    pcPad->Midplane.setValue(on);
    ui->checkBoxReversed->setEnabled(!on);
    recomputeFeature();
}

void TaskPadParameters::onReversedChanged(bool on)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());
    pcPad->Reversed.setValue(on);
    recomputeFeature();
}

void TaskPadParameters::onModeChanged(int index)
{
    PartDesign::Pad* pcPad = static_cast<PartDesign::Pad*>(vp->getObject());

    switch (index) {
        case 0:
            pcPad->Type.setValue("Length");
            // Avoid error message
            if (ui->lengthEdit->value() < Base::Quantity(Precision::Confusion(), Base::Unit::Length))
                ui->lengthEdit->setValue(5.0);
            break;
        case 1: pcPad->Type.setValue("UpToLast"); break;
        case 2: pcPad->Type.setValue("UpToFirst"); break;
        case 3: pcPad->Type.setValue("UpToFace"); break;
        default: pcPad->Type.setValue("TwoLengths");
    }

    updateUI(index);
    recomputeFeature();
}

void TaskPadParameters::onButtonFace(const bool pressed) {
    TaskSketchBasedParameters::onSelectReference(pressed, false, true, false);

    // Update button if onButtonFace() is called explicitly
    ui->buttonFace->setChecked(pressed);
}

void TaskPadParameters::onFaceName(const QString& text)
{
    ui->lineFaceName->setProperty("FaceName", TaskSketchBasedParameters::onFaceName(text));
}

double TaskPadParameters::getLength(void) const
{
    return ui->lengthEdit->value().getValue();
}

double TaskPadParameters::getLength2(void) const
{
    return ui->lengthEdit2->value().getValue();
}

double TaskPadParameters::getOffset(void) const
{
    return ui->offsetEdit->value().getValue();
}

bool   TaskPadParameters::getReversed(void) const
{
    return ui->checkBoxReversed->isChecked();
}

bool   TaskPadParameters::getMidplane(void) const
{
    return ui->checkBoxMidplane->isChecked();
}

int TaskPadParameters::getMode(void) const
{
    return ui->changeMode->currentIndex();
}

QString TaskPadParameters::getFaceName(void) const
{
    if (getMode() == 3) {
        QString faceName = ui->lineFaceName->property("FaceName").toString();
        if (!faceName.isEmpty()) {
            return getFaceReference(ui->lineFaceName->text(), faceName);
        }
    }
    return QString();
}

TaskPadParameters::~TaskPadParameters()
{
    delete ui;
}

void TaskPadParameters::changeEvent(QEvent *e)
{
    TaskBox::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        ui->lengthEdit->blockSignals(true);
        ui->lengthEdit2->blockSignals(true);
        ui->offsetEdit->blockSignals(true);
        ui->lineFaceName->blockSignals(true);
        ui->changeMode->blockSignals(true);
        int index = ui->changeMode->currentIndex();
        ui->retranslateUi(proxy);
        ui->changeMode->clear();
        ui->changeMode->addItem(tr("Dimension"));
        ui->changeMode->addItem(tr("To last"));
        ui->changeMode->addItem(tr("To first"));
        ui->changeMode->addItem(tr("Up to face"));
        ui->changeMode->addItem(tr("Two dimensions"));
        ui->changeMode->setCurrentIndex(index);

        QStringList parts = ui->lineFaceName->text().split(QChar::fromLatin1(':'));
        QByteArray upToFace = ui->lineFaceName->property("FaceName").toByteArray();
        int faceId = -1;
        bool ok = false;
        if (upToFace.indexOf("Face") == 0) {
            faceId = upToFace.remove(0,4).toInt(&ok);
        }
#if QT_VERSION >= 0x040700
        ui->lineFaceName->setPlaceholderText(tr("No face selected"));
#endif
        ui->lineFaceName->setText(ok ?
                                  parts[0] + QString::fromLatin1(":") + tr("Face") + QString::number(faceId) :
                                  tr(""));
        ui->lengthEdit->blockSignals(false);
        ui->lengthEdit2->blockSignals(false);
        ui->offsetEdit->blockSignals(false);
        ui->lineFaceName->blockSignals(false);
        ui->changeMode->blockSignals(false);
    }
}

void TaskPadParameters::saveHistory(void)
{
    // save the user values to history
    ui->lengthEdit->pushToHistory();
    ui->lengthEdit2->pushToHistory();
    ui->offsetEdit->pushToHistory();
}

void TaskPadParameters::apply()
{
    std::string name = vp->getObject()->getNameInDocument();
    const char * cname = name.c_str();

    ui->lengthEdit->apply();
    ui->lengthEdit2->apply();

    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.Type = %u", cname, getMode());
    QString facename = getFaceName();

    // TODO get rid of this if (2015-12-05, Fat-Zer)
    if (!facename.isEmpty()) {
        Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.UpToFace = %s",
                cname, facename.toLatin1().data());
    } else {
        Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.UpToFace = None", cname);
    }
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.Reversed = %i", cname, getReversed()?1:0);
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.Midplane = %i", cname, getMidplane()?1:0);
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.Offset = %f", name.c_str(), getOffset());
}

//**************************************************************************
//**************************************************************************
// TaskDialog
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskDlgPadParameters::TaskDlgPadParameters(ViewProviderPad *PadView, bool /*newObj*/)
    : TaskDlgSketchBasedParameters(PadView)
{
    assert(vp);
    Content.push_back ( new TaskPadParameters(PadView ) );
}

//==== calls from the TaskView ===============================================================

#include "moc_TaskPadParameters.cpp"
