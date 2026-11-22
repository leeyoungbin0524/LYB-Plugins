#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PitchAssist.h"

class LiveVocal final : public juce::AudioProcessor {
public:
    LiveVocal() : AudioProcessor(BusesProperties()
        .withInput("Microphone",juce::AudioChannelSet::mono(),true)
        .withOutput("Vocal",juce::AudioChannelSet::stereo(),true)),
        state(*this,nullptr,"LYBLiveVocal",layout()) {
        for(int i=0;i<count;++i) params[i]=state.getRawParameterValue(ids[i]);
    }
    const juce::String getName() const override {return "LYB Live Vocal";}
    bool acceptsMidi() const override {return false;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 10;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return "Gentle Ballad";}
    void changeProgramName(int,const juce::String&) override {}
    bool hasEditor() const override {return true;}
    juce::AudioProcessorEditor* createEditor() override {
        return new juce::GenericAudioProcessorEditor(*this);
    }
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainInputChannelSet()==juce::AudioChannelSet::mono()
            && (b.getMainOutputChannelSet()==juce::AudioChannelSet::mono()
                || b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
    }
    void prepareToPlay(double sampleRate,int) override {
        sr=sampleRate; pitch.prepare(sr);
        setLatencySamples(PitchAssist::latency);
        reverb.setSampleRate(sr); reverb.reset();
        delay.assign(size_t(std::ceil(sr*1.2))+2,0); delayPos=0;
        hpLow=low=highLow=sibLow=env=sibEnv=0; gain=gate=1;
        for(int i=0;i<count;++i) {
            smooth[i].reset(sr,.025);smooth[i].setCurrentAndTargetValue(params[i]->load());
        }
    }
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) override {
        juce::ScopedNoDenormals noDenormals;
        const int samples=buffer.getNumSamples(), channels=buffer.getNumChannels();
        if(channels==0 || delay.empty()) return;
        for(int i=0;i<count;++i) smooth[i].setTargetValue(params[i]->load());
        const bool enabled=params[pitchOn]->load()>.5f;
        const int key=int(params[root]->load()), mode=int(params[scale]->load());
        juce::Reverb::Parameters rv;
        rv.roomSize=params[room]->load();rv.damping=.55f;rv.width=1;
        rv.wetLevel=params[verb]->load()*.33f;rv.dryLevel=1;rv.freezeMode=0;
        reverb.setParameters(rv);
        auto* left=buffer.getWritePointer(0);
        auto* right=channels>1?buffer.getWritePointer(1):nullptr;
        for(int n=0;n<samples;++n) {
            float p[count];for(int i=0;i<count;++i)p[i]=smooth[i].getNextValue();
            float x=std::isfinite(left[n])?left[n]*db(p[input]):0;
            // First-order low cut, deliberately minimum-phase.
            hpLow += pole(p[cut])*(x-hpLow); x-=hpLow;
            x=pitch.process(x,enabled?p[assist]:0,p[response],key,mode);
            const float a=std::abs(x);
            env += poleTime(a>env?5:100)*(a-env);
            float level=20*std::log10(std::max(env,1e-8f));
            float gateDb=std::clamp((level-p[gateThreshold])*.5f,-18.f,0.f);
            gate += poleTime(gateDb<0?100:5)*(db(gateDb)-gate);
            x*=gate;
            // Three complementary bands: body (<250), presence (250-4000), air (>4000).
            low+=pole(250)*(x-low);highLow+=pole(4000)*(x-highLow);
            x=low*db(p[body])+(highLow-low)*db(p[presence])+(x-highLow)*db(p[air]);
            float over=std::max(0.f,level-p[threshold]);
            float gainDb=-over*(1-1/p[ratio]);
            float wanted=db(gainDb);
            gain+=poleTime(wanted<gain?p[attack]:p[release])*(wanted-gain);
            x*=gain*db(p[makeup]);
            sibLow+=pole(5500)*(x-sibLow);
            float high=x-sibLow;
            sibEnv+=poleTime(std::abs(high)>sibEnv?1:65)*(std::abs(high)-sibEnv);
            float sibDb=20*std::log10(std::max(sibEnv,1e-8f));
            float reduction=std::clamp((sibDb+30)*.65f,0.f,p[deess]);
            x=sibLow+high*db(-reduction);
            float drive=1+p[warmth]*2;
            float saturated=std::tanh(x*drive)/drive;
            x+=p[warmth]*(saturated-x);
            // Feedback echo; bounded feedback and interpolated read position.
            double readPos=double(delayPos)-sr*p[delayMs]*.001;
            while(readPos<0)readPos+=double(delay.size());
            auto i=size_t(readPos)%delay.size();float f=float(readPos-std::floor(readPos));
            float echo=delay[i]+f*(delay[(i+1)%delay.size()]-delay[i]);
            delay[delayPos]=x+echo*.25f;delayPos=(delayPos+1)%delay.size();
            x+=echo*p[delayMix];left[n]=x;if(right)right[n]=x;
        }
        if(right)reverb.processStereo(left,right,samples);else reverb.processMono(left,samples);
        float outputGain=db(params[output]->load());
        // Sample-peak ceiling only, not a lookahead/true-peak limiter.
        for(int c=0;c<channels;++c) {
            auto* data=buffer.getWritePointer(c);
            for(int n=0;n<samples;++n) {
                float x=data[n]*outputGain;
                data[n]=std::isfinite(x)?std::clamp(x,-.89125f,.89125f):0;
            }
        }
    }
    void getStateInformation(juce::MemoryBlock& dest) override {
        auto xml=state.copyState().createXml();copyXmlToBinary(*xml,dest);
    }
    void setStateInformation(const void* data,int size) override {
        auto xml=getXmlFromBinary(data,size);
        if(xml && xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));
    }
private:
    enum {input,cut,gateThreshold,body,presence,air,threshold,ratio,attack,release,makeup,
          deess,warmth,verb,room,delayMs,delayMix,output,pitchOn,assist,response,root,scale,count};
    inline static const char* ids[count]={"input","cut","gate","body","presence","air","threshold","ratio",
        "attack","release","makeup","deess","warmth","verb","room","delayMs","delayMix","output",
        "pitchOn","assist","response","root","scale"};
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout l;
        auto add=[&](int i,const char* name,float lo,float hi,float value) {
            l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ids[i],1},name,
                juce::NormalisableRange<float>{lo,hi},value));
        };
        add(input,"Input (dB)",-24,24,0);add(cut,"Low cut (Hz)",40,180,80);
        add(gateThreshold,"Expander threshold (dB)",-80,-30,-60);
        add(body,"Body (dB)",-12,12,-1);add(presence,"Presence (dB)",-12,12,1);
        add(air,"Air (dB)",-12,12,1);add(threshold,"Compressor threshold (dB)",-40,0,-18);
        add(ratio,"Compressor ratio",1,6,2.5);add(attack,"Attack (ms)",1,80,20);
        add(release,"Release (ms)",30,500,160);add(makeup,"Makeup (dB)",0,18,2);
        add(deess,"De-esser max reduction (dB)",0,12,4);add(warmth,"Warmth",0,1,.1);
        add(verb,"Reverb",0,1,.15);add(room,"Reverb size",0,1,.65);
        add(delayMs,"Delay time (ms)",60,1000,320);add(delayMix,"Delay",0,.5,.06);
        add(output,"Output (dB)",-24,6,-3);
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ids[pitchOn],1},"Pitch assist (experimental)",true));
        add(assist,"Pitch assistance (%)",0,60,25);add(response,"Pitch response (ms)",100,350,180);
        l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ids[root],1},"Song key",
            juce::StringArray{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"},0));
        l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ids[scale],1},"Scale",
            juce::StringArray{"Chromatic","Major","Natural minor"},0));
        return l;
    }
    static float db(float value){return std::pow(10.f,value/20.f);}
    float pole(float hz)const{return float(1-std::exp(-6.28318530718*std::min(double(hz),sr*.45)/sr));}
    float poleTime(float ms)const{return float(1-std::exp(-1/(sr*ms*.001)));}
    juce::AudioProcessorValueTreeState state;
    std::array<std::atomic<float>*,count> params{};
    std::array<juce::SmoothedValue<float>,count> smooth;
    PitchAssist pitch;
    juce::Reverb reverb;
    std::vector<float> delay;
    size_t delayPos=0;
    double sr=48000;
    float hpLow=0,low=0,highLow=0,sibLow=0,env=0,sibEnv=0,gain=1,gate=1;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LiveVocal();}
