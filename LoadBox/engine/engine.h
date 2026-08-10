/*
 * engine.h
 *
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 * 
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>

#ifdef __SSE__
 #include <immintrin.h>
 #ifndef _IMMINTRIN_H_INCLUDED
  #include <fxsrintrin.h>
 #endif
 #ifdef __SSE3__
  #ifndef _PMMINTRIN_H_INCLUDED
   #include <pmmintrin.h>
  #endif
 #else
  #ifndef _XMMINTRIN_H_INCLUDED
   #include <xmmintrin.h>
  #endif
 #endif //__SSE3__
#endif //__SSE__

#include "dcblocker.cc"
#include "cdelay.cc"

#include "NeuralModelLoader.h"
#include "fftconvolver.h"

#pragma once

#ifndef ENGINE_H_
#define ENGINE_H_
namespace irloader {

/////////////////////////// DENORMAL PROTECTION   //////////////////////

class DenormalProtection {
private:
#ifdef USE_SSE
    uint32_t  mxcsr_mask;
    uint32_t  mxcsr;
    uint32_t  old_mxcsr;
#endif

public:
    inline void set_() {
#ifdef USE_SSE
        old_mxcsr = _mm_getcsr();
        mxcsr = old_mxcsr;
        _mm_setcsr((mxcsr | _MM_DENORMALS_ZERO_MASK | _MM_FLUSH_ZERO_MASK) & mxcsr_mask);
#endif
    };
    inline void reset_() {
#ifdef USE_SSE
        _mm_setcsr(old_mxcsr);
#endif
    };

    inline DenormalProtection() {
#ifdef USE_SSE
        mxcsr_mask = 0xffbf; // Default MXCSR mask
        mxcsr      = 0;
        uint8_t fxsave[512] __attribute__ ((aligned (16)));

        memset(fxsave, 0, sizeof(fxsave));
        __builtin_ia32_fxsave(&fxsave);
        uint32_t mask = *(reinterpret_cast<uint32_t *>(&fxsave[0x1c]));
        if (mask != 0)
            mxcsr_mask = mask;
#endif
    };

    inline ~DenormalProtection() {};
};

class Engine
{
public:
    ParallelThread               xrworker; // handles (re)loading of IR/NAM files
    NeuralModelLoader            slotC; // NAM "outboard profile", left / IR-L
    NeuralModelLoader            slotD; // NAM "outboard profile", right / IR-R
    ConvolverSelector            conv;  // IR convolver, left
    ConvolverSelector            conv1; // IR convolver, right

    float                        IRoutputGain;
    float                        IRoutputGain1;
    float                        MasterOutGain;
    float                        IRmix;
    float                        latency;
    float                        XrunCounter;

    uint32_t                     bypass;
    uint32_t                     s_rate;
    uint32_t                     bufsize;
    uint32_t                     IRmode;
    int                          phaseOffset;

    std::string                  ir_file;
    std::string                  ir_file1;

    std::atomic<bool>            _notify_ui;
    std::atomic<bool>            _neuralC;
    std::atomic<bool>            _neuralD;
    std::atomic<int>             _cd;

    inline Engine();
    inline ~Engine();

    inline void setSampleRate(uint32_t rate);
    inline void init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_);
    inline void clean_up();
    inline void do_work_mono();
    inline void process(uint32_t n_samples, float* output, float* output1);

private:
    ParallelThread                pro;      // runs the right channel in parallel
    dcblocker::Dsp*               dcb;
    dcblocker::Dsp*               dcb1;
    cdeleay::Dsp*                 pdelay;   // phase correction between L/R when mixed to mono
    DenormalProtection            MXCSR;
    std::condition_variable       Sync;
    std::mutex                    WMutex;

    float*                        _bufb;

    double                        fRec1[2]; // IR out gain L
    double                        fRec5[2]; // IR out gain R
    double                        fRec6[2]; // Mix
    double                        fRec7[2]; // Master

    inline void processConv1();
    static bool endsWith(const std::string& str, const std::string& suffix);
    inline void setModel(NeuralModelLoader *slot,
                std::string *file, std::atomic<bool> *set);

    inline void setIRFile(ConvolverSelector *co, std::string *file);
};

inline Engine::Engine() :
    slotC(&Sync),
    slotD(&Sync),
    conv(),
    conv1(),
    xrworker(),
    pro(),
    dcb(dcblocker::plugin()),
    dcb1(dcblocker::plugin()),
    pdelay(cdeleay::plugin()),
    _bufb(0) {
        bufsize = 0;
        phaseOffset = 0;
        bypass = 0;
        IRmode = 0;
        IRoutputGain = 0.0;
        IRoutputGain1 = 0.0;
        MasterOutGain = 0.0;
        IRmix = 0.5;
        latency = 0.0;
        XrunCounter = 0.0;

        ir_file = "None";
        ir_file1 = "None";
        xrworker.start();
        pro.start();

        _neuralC.store(false, std::memory_order_release);
        _neuralD.store(false, std::memory_order_release);
};

inline Engine::~Engine(){
    xrworker.stop();
    pro.stop();

    dcb->del_instance(dcb);
    dcb1->del_instance(dcb1);
    pdelay->del_instance(pdelay);
    slotC.cleanUp();
    slotD.cleanUp();
    conv.stop_process();
    conv.cleanup();
    conv1.stop_process();
    conv1.cleanup();
};

inline void Engine::setSampleRate(uint32_t rate) {
    s_rate = rate;
    dcb->init(rate);
    dcb1->init(rate);
    slotC.init(rate);
    slotD.init(rate);
}

inline void Engine::init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_) {
    setSampleRate(rate);

    _notify_ui.store(false, std::memory_order_release);
    _cd.store(0, std::memory_order_release);

    xrworker.setThreadName("IR-Worker");
    xrworker.set<Engine, &Engine::do_work_mono>(this);

    pro.setThreadName("RT-Parallel");
    pro.setPriority(rt_prio_, rt_policy_);
    pro.set<Engine, &Engine::processConv1>(this);

    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec1[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec5[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec6[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec7[l0] = 0.0;
};

inline void Engine::clean_up() {
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec1[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec5[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec6[l0] = 0.0;
    for (int l0 = 0; l0 < 2; l0 = l0 + 1) fRec7[l0] = 0.0;
}

inline void Engine::setModel(NeuralModelLoader *slot,
                std::string *file, std::atomic<bool> *set) {
    if ((*file).compare(slot->getModelFile()) != 0) {
        slot->setModelFile(*file);
        if (!slot->loadModel()) {
            *file = "None";
            set->store(false, std::memory_order_release);
        } else {
            set->store(true, std::memory_order_release);
        }
    }
}

bool Engine::endsWith(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline void Engine::setIRFile(ConvolverSelector *co, std::string *file) {
    if (co->is_runnable()) {
        co->set_not_runnable();
        co->stop_process();
        std::unique_lock<std::mutex> lk(WMutex);
        Sync.wait_for(lk, std::chrono::milliseconds(160));
    }

    co->cleanup();
    co->set_samplerate(s_rate);
    co->set_buffersize(bufsize);
    if (endsWith(*file, "nam")) {
        if (co == &conv) setModel(&slotC, file, &_neuralC);
        else if (co == &conv1) setModel(&slotD, file, &_neuralD);
    } else {
        if (co == &conv) _neuralC.store(false, std::memory_order_release);
        else if (co == &conv1) _neuralD.store(false, std::memory_order_release);
        if (*file != "None") {
            co->configure(*file, 1.0, 0, 0, 0, 0, 0);
            while (!co->checkstate());
            if(!co->start(0, 0)) {
                *file = "None";
            }
        }
    }
    // get phase correction for Mix mode
    int cp = _neuralC.load(std::memory_order_acquire) ? slotC.getPhaseOffset() : 0;
    int dp = _neuralD.load(std::memory_order_acquire) ? slotD.getPhaseOffset() : 0;
    phaseOffset = dp - cp;
    pdelay->set(phaseOffset);
    pdelay->clear_state_f();
}

void Engine::do_work_mono() {
    // set ir/nam files
    if (_cd.load(std::memory_order_acquire) == 1) {
        setIRFile(&conv, &ir_file);
    } else if (_cd.load(std::memory_order_acquire) == 2) {
        setIRFile(&conv1, &ir_file1);
    } else if (_cd.load(std::memory_order_acquire) > 2) {
        setIRFile(&conv, &ir_file);
        setIRFile(&conv1, &ir_file1);
    }

    slotC.setMaxBufferSize(bufsize * 2);
    slotD.setMaxBufferSize(bufsize * 2);

    _cd.store(0, std::memory_order_release);
    _notify_ui.store(true, std::memory_order_release);
}

// process right channel (conv1 / slotD) in parallel thread
inline void Engine::processConv1() {
    if (_neuralD.load(std::memory_order_acquire)) {
        slotD.compute(bufsize, _bufb, _bufb);
    } else if (conv1.is_runnable()) {
        conv1.compute(bufsize, _bufb, _bufb);
    }
}

inline void Engine::process(uint32_t n_samples, float* output, float* output1) {
    if (n_samples < 1) return;
    if (!bypass) {
        Sync.notify_all();
        return;
    }
    MXCSR.set_();

    double fSlow1 = 0.0010000000000000009 * std::pow(1e+01, 0.05 * double(IRoutputGain));
    double fSlow5 = 0.0010000000000000009 * std::pow(1e+01, 0.05 * double(IRoutputGain1));

    bufsize = n_samples;

    // bufa = left input channel, bufb = right input channel
    float bufa[n_samples];
    memcpy(bufa, output, n_samples*sizeof(float));
    float bufb[n_samples];
    memcpy(bufb, output1, n_samples*sizeof(float));

    // process right channel (conv1 or slotD) in parallel thread
    _bufb = bufb;
    if (conv1.is_runnable() || _neuralD.load(std::memory_order_acquire)) {
        if (pro.getProcess()) {
            pro.runProcess();
        } else {
            XrunCounter += 1;
            _notify_ui.store(true, std::memory_order_release);
        }
    }

    // process left channel (conv or slotC)
    if (_neuralC.load(std::memory_order_acquire)) {
        slotC.compute(n_samples, bufa, bufa);
    } else if (conv.is_runnable()) {
        conv.compute(n_samples, bufa, bufa);
    }

    // wait for the parallel processed right channel when needed
    if (conv1.is_runnable() || _neuralD.load(std::memory_order_acquire)) {
        if (!pro.processWait()) {
            XrunCounter += 1;
            _notify_ui.store(true, std::memory_order_release);
        }
    }

    if (IRmode == 0) { // Stereo mode: independent L/R IR/NAM
        for (uint32_t i0 = 0; i0 < n_samples; i0 = i0 + 1) {
            fRec1[0] = fSlow1 + 0.999 * fRec1[1];
            output[i0] = bufa[i0] * fRec1[0];
            fRec1[1] = fRec1[0];
        }
        for (uint32_t i0 = 0; i0 < n_samples; i0 = i0 + 1) {
            fRec5[0] = fSlow5 + 0.999 * fRec5[1];
            output1[i0] = bufb[i0] * fRec5[0];
            fRec5[1] = fRec5[0];
        }
        dcb->compute(n_samples, output, output);
        dcb1->compute(n_samples, output1, output1);
    } else { // Mix mode: L+R summed to mono, phase corrected, on both outputs
        double fSlow7 = 0.0010000000000000009 * std::pow(1e+01, 0.05 * double(MasterOutGain));
        if ((conv.is_runnable() || _neuralC.load(std::memory_order_acquire)) &&
                (conv1.is_runnable() || _neuralD.load(std::memory_order_acquire))) {
            // phase correction: delay whichever channel is "ahead" so both
            // channels line up before being summed to mono
            if (phaseOffset) {
                if (phaseOffset < 0) pdelay->compute(n_samples, bufa, bufa); // left (slotC/conv) is ahead
                else pdelay->compute(n_samples, bufb, bufb);                // right (slotD/conv1) is ahead
            }
            double fSlow6 = 0.0010000000000000009 * double(IRmix);
            for (uint32_t i0 = 0; i0 < n_samples; i0 = i0 + 1) {
                fRec6[0] = fSlow6 + 0.999 * fRec6[1];
                output[i0] = bufa[i0] * (1.0 - fRec6[0]) + bufb[i0] * fRec6[0];
                fRec6[1] = fRec6[0];
            }
        } else if (conv.is_runnable() || _neuralC.load(std::memory_order_acquire)) {
            memcpy(output, bufa, n_samples*sizeof(float));
        } else if (conv1.is_runnable() || _neuralD.load(std::memory_order_acquire)) {
            memcpy(output, bufb, n_samples*sizeof(float));
        }
        for (uint32_t i0 = 0; i0 < n_samples; i0 = i0 + 1) {
            fRec7[0] = fSlow7 + 0.999 * fRec7[1];
            output[i0] *= fRec7[0];
            fRec7[1] = fRec7[0];
        }
        dcb->compute(n_samples, output, output);
        memcpy(output1, output, n_samples*sizeof(float));
    }

    Sync.notify_all();
    MXCSR.reset_();
}

}; // end namespace irloader
#endif
