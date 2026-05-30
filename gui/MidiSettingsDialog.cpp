#include "MidiSettingsDialog.h"
#include "MidiController.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

namespace gui {

MidiSettingsDialog::MidiSettingsDialog(MidiController* midi, QWidget* parent)
    : QDialog(parent), midi_(midi)
{
    setWindowTitle("MIDI Settings");
    setMinimumWidth(400);

    auto* vbox = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    inCombo_  = new QComboBox(this);
    outCombo_ = new QComboBox(this);

    inCombo_->addItem("(none)", -1);
    const auto inPorts = midi_->inputPortNames();
    for (int i = 0; i < static_cast<int>(inPorts.size()); ++i)
        inCombo_->addItem(inPorts[static_cast<size_t>(i)], i);

    outCombo_->addItem("(none)", -1);
    const auto outPorts = midi_->outputPortNames();
    for (int i = 0; i < static_cast<int>(outPorts.size()); ++i)
        outCombo_->addItem(outPorts[static_cast<size_t>(i)], i);

    form->addRow("MIDI Input:",  inCombo_);
    form->addRow("MIDI Output:", outCombo_);
    vbox->addLayout(form);

    auto* hint = new QLabel(
        "nanoKONTROL2: select the port named \"nanoKONTROL2\".\n"
        "X-Touch Mini (GM Mode): select \"X-TOUCH MINI\".\n"
        "Default mapping: faders 1\xe2\x80\x937 \xe2\x86\x92 EQ bands; "
        "knobs 1\xe2\x80\x934 \xe2\x86\x92 Sat/MixbusThresh/MixbusMakeup/LimCeiling; "
        "Play/Stop transport.",
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #666; font-size: 9pt;");
    vbox->addWidget(hint);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &MidiSettingsDialog::onApply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vbox->addWidget(buttons);

    loadSettings();
}

void MidiSettingsDialog::loadSettings() {
    QSettings s("MasterTweak", "MasterTweak");
    const QString savedIn  = s.value("midi/inputPort").toString();
    const QString savedOut = s.value("midi/outputPort").toString();

    for (int i = 1; i < inCombo_->count(); ++i)
        if (inCombo_->itemText(i) == savedIn) { inCombo_->setCurrentIndex(i); break; }
    for (int i = 1; i < outCombo_->count(); ++i)
        if (outCombo_->itemText(i) == savedOut) { outCombo_->setCurrentIndex(i); break; }
}

void MidiSettingsDialog::onApply() {
    const int inIdx  = inCombo_->currentData().toInt();
    const int outIdx = outCombo_->currentData().toInt();

    if (inIdx >= 0)  midi_->openInput(static_cast<unsigned>(inIdx));
    else             midi_->closeInput();

    if (outIdx >= 0) midi_->openOutput(static_cast<unsigned>(outIdx));
    else             midi_->closeOutput();

    saveSettings(inCombo_->currentText(), outCombo_->currentText());
    accept();
}

void MidiSettingsDialog::saveSettings(const QString& inName, const QString& outName) {
    QSettings s("MasterTweak", "MasterTweak");
    s.setValue("midi/inputPort",  inName);
    s.setValue("midi/outputPort", outName);
}

} // namespace gui
