#include <stdio.h>
#include "NUC123.h"
#include "i2c_transfer.h"
#include "targetdev.h"

/*--------------------------------------------------------------------------*/
void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Enable XT1_OUT(PF0) and XT1_IN(PF1) */
    SYS->GPF_MFP |= SYS_GPF_MFP_PF0_XT1_OUT | SYS_GPF_MFP_PF1_XT1_IN;
    /* Enable Internal RC 22.1184 MHz clock */
    /* Enable external XTAL 12 MHz clock */
    CLK->PWRCON |= (CLK_PWRCON_OSC22M_EN_Msk | CLK_PWRCON_XTL12M_EN_Msk);

    /* Waiting for external XTAL clock ready */
    while (!(CLK->CLKSTATUS & CLK_PWRCON_XTL12M_EN_Msk));

    /* Set core clock as CLK_PLLCON_144MHz_HXT from PLL */
    CLK->PLLCON = CLK_PLLCON_144MHz_HXT;

    while (!(CLK->CLKSTATUS & CLK_CLKSTATUS_PLL_STB_Msk));

    CLK->CLKDIV &= ~CLK_CLKDIV_HCLK_N_Msk;
    CLK->CLKDIV |= CLK_CLKDIV_HCLK(2);
    CLK->CLKDIV &= ~CLK_CLKDIV_USB_N_Msk;
    CLK->CLKDIV |= CLK_CLKDIV_USB(3);
    CLK->CLKSEL0 &= (~CLK_CLKSEL0_HCLK_S_Msk);
    CLK->CLKSEL0 |= CLK_CLKSEL0_HCLK_S_PLL;
    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate PllClock, SystemCoreClock and CycylesPerUs automatically. */
    //SystemCoreClockUpdate();
    PllClock        = 144000000;                // PLL
    SystemCoreClock = 144000000 / 2;        // HCLK
    CyclesPerUs     = SystemCoreClock / 1000000;                   // For SYS_SysTickDelay()
    /* Enable module clock */
    CLK->APBCLK |= (CLK_APBCLK_UART1_EN_Msk | CLK_APBCLK_USBD_EN_Msk);
    CLK->AHBCLK |= CLK_AHBCLK_ISP_EN_Msk;   // (1ul << 2)
    /* Select UART module clock source */
    CLK->CLKSEL1 &= ~CLK_CLKSEL1_UART_S_Msk;
    CLK->CLKSEL1 |= CLK_CLKSEL1_UART_S_HIRC;
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set GPB multi-function pins for UART1 RXD and TXD */
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB4_UART1_RXD | SYS_GPB_MFP_PB5_UART1_TXD);
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main(void)
{
    volatile uint32_t u32BootAP;
    /* Unlock write-protected registers */
    SYS_UnlockReg();
    u32BootAP = (FMC->ISPCON & FMC_ISPCON_BS_Msk) ? 0 : 1;
    PB13 = u32BootAP;
    PB14 = !u32BootAP;
    /* Init system and multi-funcition I/O */
    SYS_Init();
    I2C_Init();
    FMC->ISPCON |= FMC_ISPCON_ISPEN_Msk;    // (1ul << 0)
    GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);

    while (DetectPin == 0) {
        I2C_SlaveRcvSendData();

        if (bI2CDataReady == TRUE) {
            ParseCmd(I2C_rcvbuf, 64);
            bI2CDataReady = FALSE;
        }
    }

    outpw(&SYS->RSTSRC, 3);//clear bit
    outpw(&FMC->ISPCON, inpw(&FMC->ISPCON) & 0xFFFFFFFC);
    outpw(&SCB->AIRCR, (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ));

    /* Trap the CPU */
    while (1);
}
