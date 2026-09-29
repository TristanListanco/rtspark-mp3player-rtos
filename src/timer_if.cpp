/*
 * timer_if.cpp - Ticker / Timeout on TIM2 and TIM5 (32-bit, APB1).
 */
#include "timer_if.h"
#include "main.h"

namespace {

/* Only integer constants here, so this table is constant-initialised and is
 * valid before any global Ticker/Timeout constructor runs (the TIM2/TIM5
 * pointer macros are casts, which would force dynamic initialisation). */
struct HwTimer {
    uint32_t base;
    IRQn_Type irq;
};

const HwTimer kTimers[] = {
    { TIM2_BASE, TIM2_IRQn },
    { TIM5_BASE, TIM5_IRQn },
};
const int kNumTimers = sizeof kTimers / sizeof kTimers[0];

/* Zero-initialised before any constructor runs */
TimerEvent *owners[kNumTimers];
bool clock_on[kNumTimers];
int next_free;

inline TIM_TypeDef *timer_regs(int slot)
{
    return reinterpret_cast<TIM_TypeDef *>(kTimers[slot].base);
}

/* Timer kernel clock: PCLK1, doubled when the APB1 prescaler is not 1 */
uint32_t apb1_timer_clock()
{
    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const bool apb1_divided = (RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1;
    return apb1_divided ? 2u * pclk1 : pclk1;
}

void enable_timer_clock(int slot)
{
    if (clock_on[slot]) {
        return;
    }
    if (kTimers[slot].base == TIM2_BASE) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    } else {
        __HAL_RCC_TIM5_CLK_ENABLE();
    }
    HAL_NVIC_SetPriority(kTimers[slot].irq, TIMER_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(kTimers[slot].irq);
    clock_on[slot] = true;
}

/* Masks all interrupts for a few register writes; nests safely in ISRs. */
class IrqLock {
public:
    IrqLock() : primask_(__get_PRIMASK()) { __disable_irq(); }
    ~IrqLock() { __set_PRIMASK(primask_); }
private:
    uint32_t primask_;
};

} // namespace

/* Objects are normally globals: the constructor only claims a timer slot.
 * The hardware is touched on the first attach(), after HAL/clock init. */
TimerEvent::TimerEvent(bool one_shot)
    : one_shot_(one_shot), slot_(-1), active_(false), callback_(nullptr)
{
    if (next_free < kNumTimers) {
        slot_ = next_free++;
        owners[slot_] = this;
    }
}

void TimerEvent::schedule(Callback cb, float seconds)
{
    if (slot_ < 0) {
        Error_Handler();                        /* more Ticker/Timeout objects than timers */
    }
    TIM_TypeDef *t = timer_regs(slot_);

    uint32_t us = (seconds <= 0.0f) ? 1u : (uint32_t)(seconds * 1e6f + 0.5f);
    if (us == 0) {
        us = 1;
    }

    IrqLock lock;
    enable_timer_clock(slot_);

    t->CR1 = 0;                                 /* stop */
    t->DIER = 0;
    callback_ = cb;
    t->PSC = apb1_timer_clock() / 1000000u - 1u; /* 1 MHz count rate */
    t->ARR = us - 1u;
    t->CNT = 0;
    t->EGR = TIM_EGR_UG;                        /* load PSC now */
    t->SR = 0;                                  /* drop the UG update flag */
    NVIC_ClearPendingIRQ(kTimers[slot_].irq);
    t->DIER = TIM_DIER_UIE;
    t->CR1 = TIM_CR1_URS | (one_shot_ ? TIM_CR1_OPM : 0u) | TIM_CR1_CEN;
    active_ = true;
}

void TimerEvent::detach()
{
    if (slot_ < 0) {
        return;
    }

    IrqLock lock;
    if (clock_on[slot_]) {
        TIM_TypeDef *t = timer_regs(slot_);
        t->CR1 = 0;
        t->DIER = 0;
        t->SR = 0;
        NVIC_ClearPendingIRQ(kTimers[slot_].irq);
    }
    active_ = false;
}

void TimerEvent::handle_irq(int slot)
{
    TIM_TypeDef *t = timer_regs(slot);
    if ((t->SR & TIM_SR_UIF) == 0) {
        return;
    }
    t->SR = ~TIM_SR_UIF;                        /* rc_w0: write 0 to clear */

    TimerEvent *ev = owners[slot];
    if (ev == nullptr) {
        return;
    }
    if (ev->one_shot_) {
        t->DIER = 0;                            /* OPM already stopped the counter */
        ev->active_ = false;
    }
    const Callback cb = ev->callback_;
    if (cb != nullptr) {
        cb();                                   /* may call attach() again */
    }
}

extern "C" void TIM2_IRQHandler(void)
{
    TimerEvent::handle_irq(0);
}

extern "C" void TIM5_IRQHandler(void)
{
    TimerEvent::handle_irq(1);
}
