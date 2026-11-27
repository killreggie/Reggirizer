// Developer tool: renders the plugin editor to a PNG without a DAW.
// Usage: UISnapshot out.png
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                   : juce::File::getCurrentWorkingDirectory().getChildFile ("ui.png"));

    ReggirizerProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Sing a slightly sharp A4 through it so the displays have something to show.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double ph = 0.0;
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    for (int block = 0; block < 120; ++block)
    {
        for (int n = 0; n < 512; ++n)
        {
            const double hz = 452.0 * std::pow (2.0, 0.3 / 12.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * (block * 512 + n) / 48000.0));
            ph += juce::MathConstants<double>::twoPi * hz / 48000.0;
            buf.setSample (0, n, 0.4f * (float) std::sin (ph));
            buf.setSample (1, n, 0.4f * (float) std::sin (ph));
        }
        proc.processBlock (buf, midi);
        if (block % 4 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (34); // let the 30 Hz displays tick
    }

    auto save = [&] (const juce::File& f)
    {
        const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        f.deleteFile();
        juce::FileOutputStream os (f);
        const bool good = os.openedOk() && juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s %s (%dx%d)\n", good ? "wrote" : "FAILED", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        return good;
    };

    // Open the dreidel pop-up: one shot mid-spin, one after it lands.
    for (auto* child : editor->getChildren())
        if (auto* pop = dynamic_cast<DreidelPopup*> (child))
        {
            pop->open();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (900);
            save (out.getSiblingFile (out.getFileNameWithoutExtension() + "-dreidel-spin.png"));
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5200);
            save (out.getSiblingFile (out.getFileNameWithoutExtension() + "-dreidel-landed.png"));
            std::printf ("dreidel landed on %d, speed now %.1f ms\n", pop->getResult(), proc.apvts.getRawParameterValue ("speed")->load());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (2500);
            pop->close();
            break; // open() brings it to the front, which reorders the child list
        }

    bool ok = save (out);

    // Second shot: the flanger page, by clicking its tab.
    if (auto* tab = dynamic_cast<juce::Button*> (editor->findChildWithID ("flangerTab")))
    {
        tab->triggerClick();
        // Show it working: turn the flanger on with some feedback and run audio through.
        auto set = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set ("flgOn", 1.0f); set ("flgFeedback", 45.0f); set ("flgDepth", 40.0f);
        for (int block = 0; block < 40; ++block)
            proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        ok = save (out.getSiblingFile (out.getFileNameWithoutExtension() + "-flanger.png")) && ok;
    }
    editor.reset();
    return ok ? 0 : 1;
}
