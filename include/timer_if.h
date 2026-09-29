/*
 * timer_if.h - mbed-style Ticker and Timeout on STM32 hardware timers.
 *
 *   Ticker  : calls a function repeatedly at a fixed interval (recurring interrupt)
 *   Timeout : calls a function once after a delay (one-shot interrupt)
 *
 * Same usage as in the lab handout (section 4.2):
 *
 *     Ticker  tickerflipper;
 *     Timeout timeoutflipper;
 *     tickerflipper.attach(&tickerFlip, 2.0);     // every 2 s
 *     timeoutflipper.attach(&timeoutFlipper, 2.0); // once, after 2 s
 *
 * Each object owns one 32-bit timer (TIM2, TIM5) counting at 1 MHz, so
 * intervals from 1 us to more than an hour are supported. Callbacks run in
 * interrupt context (priority TIMER_IRQ_PRIORITY): keep them short and don't
 * call blocking FreeRTOS APIs from them. Calling attach() again, including
 * from inside the callback, restarts the timer with the new interval.
 */
#ifndef TIMER_IF_H
#define TIMER_IF_H

#ifdef __cplusplus

class TimerEvent {
public:
    typedef void (*Callback)(void);

    /* Stops the timer; the callback won't be called again. */
    void detach();

    bool attached() const { return active_; }

    /* Dispatch from the TIMx_IRQHandler functions (internal). */
    static void handle_irq(int slot);

protected:
    explicit TimerEvent(bool one_shot);
    void schedule(Callback cb, float seconds);

private:
    TimerEvent(const TimerEvent &);             /* non-copyable */
    TimerEvent &operator=(const TimerEvent &);

    const bool one_shot_;
    int slot_;                                  /* hardware timer index, -1 if none */
    volatile bool active_;
    volatile Callback callback_;
};

class Ticker : public TimerEvent {
public:
    Ticker() : TimerEvent(false) {}
    /* Calls fn every `seconds` seconds until detach(). */
    void attach(Callback fn, float seconds) { schedule(fn, seconds); }
};

class Timeout : public TimerEvent {
public:
    Timeout() : TimerEvent(true) {}
    /* Calls fn once, `seconds` seconds from now. */
    void attach(Callback fn, float seconds) { schedule(fn, seconds); }
};

#endif /* __cplusplus */

#endif /* TIMER_IF_H */
