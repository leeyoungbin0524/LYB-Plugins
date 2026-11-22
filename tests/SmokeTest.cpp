#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PitchAssist.h"
#include <iostream>
#include <stdexcept>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
static void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
int main() {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        // Fixed-delay bypass: checks latency and reset without inspecting internals.
        for(double rate : {44100.,48000.,96000.}) {
            PitchAssist pitch;pitch.prepare(rate);
            for(int i=0;i<4096;++i) {
                float y=pitch.process(i==0?.5f:0.f,0,180,0,0);
                require(std::isfinite(y),"Pitch produced non-finite sample");
                require(std::abs(y-(i==PitchAssist::latency?.5f:0.f))<1e-6f,"Pitch bypass latency mismatch");
            }
            pitch.prepare(rate);
            for(int i=0;i<int(rate*2);++i) {
                float x=.2f*std::sin(float(i*6.283185307179586*445/rate));
                float y=pitch.process(x,25,180,0,0);
                require(std::isfinite(y) && std::abs(y)<=.201f,"Pitch output invalid");
            }
        }
        std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
        juce::MemoryBlock state;
        p->getStateInformation(state);
        require(state.getSize()>0,"State is empty");
        auto* first=p->getParameters()[0];float before=first->getValue();
        first->setValueNotifyingHost(.75f);
        p->setStateInformation(state.getData(),int(state.getSize()));
        require(std::abs(first->getValue()-before)<1e-5f,"State restore failed");
        for(double rate : {44100.,48000.,96000.}) for(int outputs : {1,2}) {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());
            layout.outputBuses.add(outputs==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
            require(p->setBusesLayout(layout),"Unsupported required layout");
            p->setRateAndBufferSizeDetails(rate,512);p->prepareToPlay(rate,512);
            juce::MidiBuffer midi;
            double clock=0;
            for(int block=0;block<180;++block) {
                const int size=block%3==0?32:block%3==1?127:512;
                juce::AudioBuffer<float> b(outputs,size);b.clear();
                for(int n=0;n<size;++n) {
                    b.setSample(0,n,block<20?0.f:float(.25*std::sin(clock*6.283185307179586*220/rate)));
                    ++clock;
                }
                p->processBlock(b,midi);
                for(int c=0;c<outputs;++c)for(int n=0;n<size;++n) {
                    float x=b.getSample(c,n);
                    require(std::isfinite(x) && std::abs(x)<=.89126f,"Plugin output invalid");
                }
            }
            p->releaseResources();
        }
        std::cout<<"PASS: delay, finite/bounded audio, variable blocks, rates, layouts, state restore\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
