#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Experimental monophonic assistance. No allocation in process().
// Downsampled YIN estimate + slow pitch-centre correction + dual delay heads.
// This prototype is NOT formant preserving. Validate on real vocals before use.
class PitchAssist {
public:
    static constexpr int latency = 1024;
    void prepare(double rate) {
        sr = rate; stride = std::max(1, int(sr / 12000.0));
        history.fill(0); ring.fill(0); diff.fill(0);
        pos = write = tick = hops = 0; centre = target = correction = 0;
        phase = .5; energy = 0; valid = false; held = 0;
    }
    float process(float input, float amount, float responseMs, int root, int scale) {
        ring[write] = input;
        if (++tick >= stride) {
            tick = 0; history[pos] = input; pos = (pos + 1) % N;
            if (++hops >= 128) { hops = 0; analyse(amount, root, scale); }
        }
        const double alpha = 1.0 - std::exp(-1.0 / (sr * responseMs * .001));
        correction += alpha * (target - correction);
        // Small, bounded pitch shifts. Heads always read behind write pointer.
        double ratio = std::pow(2.0, correction / 1200.0);
        phase += (1.0 - ratio) / 1024.0;
        phase -= std::floor(phase);
        const double q = phase < .5 ? phase + .5 : phase - .5;
        const double w = .5 - .5 * std::cos(6.283185307179586 * phase);
        float shifted = float(w * read(512 + phase * 1024) + (1-w) * read(512 + q * 1024));
        float dry = read(latency);
        // At unity use a fixed delay; smoothly introduce the shifted signal.
        double wet = std::min(1.0, std::abs(correction) / 3.0);
        write = (write + 1) % int(ring.size());
        return float(dry + wet * (shifted - dry));
    }
private:
    static constexpr int N = 512, W = 256;
    std::array<float,N> history{};
    std::array<float,4096> ring{};
    std::array<double,W> diff{};
    double sr=48000, phase=.5, centre=0, target=0, correction=0, energy=0;
    int stride=4,pos=0,write=0,tick=0,hops=0,held=0;
    bool valid=false;
    float read(double delay) const {
        double at = write - delay;
        while(at < 0) at += ring.size();
        int i = int(at) % int(ring.size());
        double frac = at - std::floor(at);
        return float(ring[i] + frac * (ring[(i+1)%ring.size()] - ring[i]));
    }
    float sample(int i) const { return history[(pos+i)%N]; }
    bool allowed(int note, int root, int scale) const {
        if(scale==0) return true;
        int pc = ((note-root)%12+12)%12;
        constexpr int major[]={0,2,4,5,7,9,11}, minor[]={0,2,3,5,7,8,10};
        for(int i=0;i<7;++i) if(pc==(scale==1?major[i]:minor[i])) return true;
        return false;
    }
    void analyse(float amount, int root, int scale) {
        energy=0;
        for(int j=0;j<W;++j) energy += double(sample(j))*sample(j);
        if(energy/W < 1e-6) {target=0; valid=false; held=0; return;}
        double sum=0; diff[0]=1;
        int maxLag=std::min(W-2,int(sr/stride/80.0));
        int minLag=std::max(2,int(sr/stride/900.0));
        for(int lag=1;lag<=maxLag;++lag) {
            double d=0;
            for(int j=0;j<W;++j) {double e=sample(j)-sample(j+lag);d+=e*e;}
            sum+=d; diff[lag]=sum>1e-12?d*lag/sum:1;
        }
        int best=0;
        for(int lag=minLag;lag<maxLag;++lag) if(diff[lag]<.12) {
            while(lag+1<maxLag && diff[lag+1]<diff[lag]) ++lag;
            best=lag; break;
        }
        if(!best) {target=0;valid=false;held=0;return;}
        double den=diff[best-1]-2*diff[best]+diff[best+1];
        double offset=std::abs(den)>1e-12?.5*(diff[best-1]-diff[best+1])/den:0;
        double hz=sr/stride/(best+std::clamp(offset,-.5,.5));
        double midi=69+12*std::log2(hz/440.0);
        if(!valid || std::abs(midi-centre)>.8) {centre=midi;held=0;}
        else centre += .12*(midi-centre);
        valid=true;
        // Wait for a held note; do not immediately pull attacks and slides.
        if(++held<8) {target=0;return;}
        int nearest=int(std::round(centre)); double distance=100;
        for(int n=int(std::floor(centre))-2;n<=int(std::ceil(centre))+2;++n)
            if(allowed(n,root,scale) && std::abs(n-centre)<distance) {distance=std::abs(n-centre);nearest=n;}
        double cents=(nearest-centre)*100;
        // Deadband and capture limit protect intentional pitches.
        target = std::abs(cents)<10 || std::abs(cents)>60 ? 0
            : std::clamp(cents*amount/100.0,-30.0,30.0);
    }
};
