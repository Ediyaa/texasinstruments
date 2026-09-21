#include <msp430.h>
#include "include/clk.h"

/* FLL gestoppt neu einstellen, wieder freigeben, Einschwingen abwarten */
static void clock_fll_setup(uint16_t selref_div, uint16_t flln)
{
    __bis_SR_register(SCG0);                /* FLL anhalten */
    UCSCTL0 = 0x0000;                       /* DCO- und MOD-Bits auf niedrigste Stufe */
    UCSCTL1 = CLOCK_DCORSEL;
    UCSCTL2 = FLLD_1 | flln;                /* FLLD = /2, FLLN */
    UCSCTL3 = selref_div;                   /* Referenzquelle + Referenzteiler */
    __bic_SR_register(SCG0);                /* FLL freigeben */
}

static void clock_wait_dco(void)
{
    while (UCSCTL7 & DCOFFG) {
        UCSCTL7 &= ~DCOFFG;
    }
    SFRIFG1 &= ~OFIFG;
}

/* ---------- Variante 1: FLL an XT1 (32-kHz-Quarz) ---------- */
void clock_init_xt1(void)
{
    P5SEL |= BIT4 | BIT5;                                   /* XIN / XOUT */
    UCSCTL6 = (UCSCTL6 & ~(XT1OFF | XCAP_3)) | CLOCK_XT1_XCAP;

    while (UCSCTL7 & XT1LFOFFG) {                           /* warten bis XT1 schwingt */
        UCSCTL7 &= ~XT1LFOFFG;
    }

    clock_fll_setup(SELREF__XT1CLK | FLLREFDIV__1, CLOCK_FLLN_32K);
    __delay_cycles(CLOCK_SETTLE_32K);
    clock_wait_dco();

    UCSCTL4 = SELA__XT1CLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
}

/* ---------- Variante 2: FLL an REFO (intern) ---------- */
void clock_init_refo(void)
{
    clock_fll_setup(SELREF__REFOCLK | FLLREFDIV__1, CLOCK_FLLN_32K);
    __delay_cycles(CLOCK_SETTLE_32K);
    clock_wait_dco();

    UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
}

/* ---------- Variante 3: FLL an XT2 (4-MHz-Keramikresonator) ---------- */
void clock_init_xt2(void)
{
    P5SEL |= BIT2 | BIT3;                                   /* XT2IN / XT2OUT */
    UCSCTL6 &= ~(XT2OFF | XT2DRIVE_3);                      /* XT2DRIVE_0: 4–8 MHz */

    while (UCSCTL7 & XT2OFFG) {                             /* warten bis XT2 schwingt */
        UCSCTL7 &= ~XT2OFFG;
    }

    clock_fll_setup(SELREF__XT2CLK | FLLREFDIV__16, CLOCK_FLLN_XT2);
    __delay_cycles(CLOCK_SETTLE_XT2);
    clock_wait_dco();

    UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
}