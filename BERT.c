/*
 * BERT (Bit Error Rate Tester) for PIC16F648A + CCS C
 * Version 0.4 (original: 2008/09/16)
 *
 * --- What this firmware does ---
 * - Receives an external bit stream (CLK + DATA).
 * - Internally generates the expected PN sequence using an LFSR.
 * - Compares received DATA with expected bit on each clock edge.
 * - First it performs a "sync" phase (auto polarity correction + lock),
 *   then performs a "count" phase and reports BER / error count on LCD.
 *
 * --- Notes from the original author (JP) ---
 * メニュー画面を廃し、設定を簡単にした。
 * PN9以外を測定したい場合は tap の位置（フィードバック多項式）を変える。
 *
 * --- Encoding ---
 * The original file looked like CP932/Shift-JIS. This version is intended for UTF-8 (GitHub friendly).
 */

#include <16f648A.h>
#fuses HS,NOWDT,PUT,NOPROTECT,NOMCLR
#use delay(clock=20000000)

/*
 * LCD driver:
 * This project uses CCS's LCD driver variant for PORTB.
 * Original comment: lcd.cから #define use_portb_lcd TRUE を Un-comment したもの。
 */
#include <lcd_b.c>

/* -----------------------------
 * Pin assignment (PIC16F648A)
 * -----------------------------
 * Inputs:
 *  - CLK_IN   : external clock input for bit sampling
 *  - DATA_IN  : external data input
 *  - SW_SEL   : "select / setting" button
 *  - SW_TRIG  : "trigger / start" button
 *
 * Outputs:
 *  - SYNC_LED : indicates lock/sync status (or general status LED)
 *
 * NOTE:
 * - Clock polarity and Data polarity can be inverted by flags ClockNeg/DataNeg
 *   (used as XOR masks when sampling).
 */
#define CLK_IN    PIN_A4
#define DATA_IN   PIN_A5
#define SW_SEL    PIN_A0
#define SW_TRIG   PIN_A1
#define SYNC_LED  PIN_A3

/* -----------------------------------------
 * EEPROM default contents (PIC data EEPROM)
 * -----------------------------------------
 * CCS: #ROM 0x2100 = {...} sets the initial EEPROM values.
 *
 * Address map (read/write):
 *   0: ClockNeg   (0/1) clock polarity invert flag
 *   1: DataNeg    (0/1) data polarity invert flag
 *   2: TBI        (0..5) index for TotalBits (measurement length)
 *   3: ThresError (int) threshold for sync phase (consecutive "match" count)
 *
 * Note: comment says 16F8X/16F87X/16F62X uses EEPROM from 0x2100.
 */
#ROM 0x2100 = {0,0,2,10}

/* -----------------------------
 * Globals
 * ----------------------------- */
short ClockNeg, DataNeg;     // XOR polarity flags (0/1)
int   RenzokuError;          // "consecutive match counter" during sync (JP: 連続)
int   ThresError;            // required consecutive matches to declare lock
long  ErrorBits;             // number of bit errors during counting phase
long  TotalBits;             // total bits to count during measurement (from table)
long  CountBits;             // actual counted bits

/*
 * LFSR state buffer.
 * CCS provides shift_right(array, bytes, input_bit).
 * Here tap[] is used as a bit-array-like storage (short per element).
 *
 * The current feedback implementation uses tap[7] ^ tap[11].
 * If you want PN sequence other than the current one,
 * change the feedback taps accordingly.
 */
short tap[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

/*
 * Test Bit count Index (TBI)
 * TotalBits is selected from tbit[] by TBI.
 */
int  TBI;
long const tbit[6] = {1000, 5000, 10000, 30000, 50000, 65535};

/* Speed optimization for PORTA I/O (CCS) */
#use fast_io(a)

/* --------------------------------------------------------
 * setsetting()
 * - Called when SW_TRIG is pressed (case 2 in main loop).
 * - Cycles through measurement lengths (TotalBits).
 * - Waits for SW_SEL release to avoid repeated increment.
 * -------------------------------------------------------- */
void setsetting() {
    TBI++;
    if (TBI == 6) { TBI = 0; }   // wrap-around

    TotalBits = tbit[TBI];

    delay_ms(50);
    while (input(SW_SEL));       // wait release (debounce-ish)
}

/* --------------------------------------------------------
 * countber()
 * Two-phase operation:
 *
 * (1) Sync/Lock phase:
 *   - Attempts to align LFSR expected bit with incoming DATA.
 *   - If mismatch occurs, it flips tap[15] (expected output bit)
 *     and resets the consecutive counter.
 *   - Once it achieves ThresError consecutive "matches",
 *     it declares sync and turns SYNC_LED ON.
 *
 * (2) Count phase:
 *   - Runs TotalBits comparisons and increments ErrorBits on mismatch.
 *
 * Sampling:
 *   - Waits for the configured clock edge:
 *       while(!input(CLK_IN) ^ ClockNeg)   // wait rising (considering inversion)
 *     then reads DATA and compares with tap[15] (expected bit).
 *   - Waits for clock to go low again before next iteration.
 *
 * NOTE:
 * - The XOR with DataNeg / ClockNeg provides polarity inversion support.
 * -------------------------------------------------------- */
void countber() {
    printf(lcd_putc, "\fCounting...\nｹｲｿｸﾁｭｳ...");

    RenzokuError = 0;
    ErrorBits = 0;
    CountBits = 0;

    output_low(SYNC_LED);
    delay_ms(500);

    /* -------- Sync phase --------
     * RenzokuError is used as the "consecutive match counter".
     * The for-loop variable is re-used intentionally; mismatches reset it.
     */
    for (RenzokuError = 0; RenzokuError < ThresError; RenzokuError++) {

        // Wait for (logical) rising edge
        while ( (!input(CLK_IN)) ^ ClockNeg );

        // Compare expected bit (tap[15]) with incoming DATA (polarity applied)
        if ( tap[15] == (input(DATA_IN) ^ DataNeg) ) {
            // match: do nothing
        } else {
            // mismatch: reset consecutive counter and flip expected bit to realign
            RenzokuError = 0;
            tap[15] = ~tap[15];
        }

        // Wait for (logical) falling edge / clock low
        while ( (input(CLK_IN)) ^ ClockNeg );

        // Advance LFSR state
        shift_right(tap, 2, tap[7] ^ tap[11]);
    }

    output_high(SYNC_LED);

    /* -------- Count phase -------- */
    for (CountBits = 0; CountBits < TotalBits; CountBits++) {

        while ( (!input(CLK_IN)) ^ ClockNeg );

        if ( tap[15] == (input(DATA_IN) ^ DataNeg) ) {
            // match: do nothing
        } else {
            // mismatch: count error
            ErrorBits++;
        }

        while ( (input(CLK_IN)) ^ ClockNeg );

        shift_right(tap, 2, tap[7] ^ tap[11]);
    }
}

/* --------------------------------------------------------
 * show_ber()
 * - Displays BER (%) and raw counters on LCD.
 * - Waits for either key.
 * - If SW_TRIG is pressed, immediately runs another measurement (countber()).
 * -------------------------------------------------------- */
void show_ber() {
    float fEB, fCB;

    fCB = CountBits;   // convert to float
    fEB = ErrorBits;

    // FIX: missing comma after format string in the original text
    printf(lcd_putc,
           "\fBER=%lf%% \nE=%5lu C=%5lu",
           ( (fEB / fCB) * 100.0 ),
           ErrorBits, CountBits);

    // Wait for any key
    while ( !(input(SW_SEL) || input(SW_TRIG)) );
    delay_ms(50);

    // Trigger key repeats measurement
    if (input(SW_TRIG)) { countber(); }
}

/* --------------------------------------------------------
 * main()
 * Startup behavior:
 * - Init LCD
 * - Setup TRIS for PORTA (A0,A1,A4,A5 inputs; A2,A3 outputs)
 * - Read settings from EEPROM
 * - Optional polarity toggle:
 *    - Power-on with SW_TRIG held: invert DataNeg
 *    - Power-on with SW_SEL  held: invert ClockNeg
 *
 * UI (two buttons):
 * - SW_SEL: start measurement / show screen
 * - SW_TRIG: change setting (measurement length)
 * - Both pressed: save settings to EEPROM
 * -------------------------------------------------------- */
void main() {
    delay_ms(50);
    lcd_init();

    // A0,A1,A4,A5 inputs; A2,A3 outputs (1=input, 0=output)
    set_tris_a(0b00110011);

    // Load settings from EEPROM
    ClockNeg   = read_eeprom(0);
    DataNeg    = read_eeprom(1);
    TBI        = read_eeprom(2);
    TotalBits  = tbit[TBI];
    ThresError = read_eeprom(3);

    output_low(SYNC_LED);

    // Power-on key combo to flip polarity flags (quick field adjustment)
    if (input(SW_TRIG)) { DataNeg  = ~DataNeg; }
    if (input(SW_SEL )) { ClockNeg = ~ClockNeg; }

    while (TRUE) {
        // Wait until both switches are released
        while ( input(SW_SEL) || input(SW_TRIG) );
        delay_ms(50);

        // FIX: missing commas in original text (likely copy/paste artifact)
        printf(lcd_putc,
               "\fBERT PN9 D%u-C%u\nT:%Lu S:%u",
               DataNeg, ClockNeg, TotalBits, ThresError);

        // Wait for any key
        while ( !(input(SW_SEL) || input(SW_TRIG)) );
        delay_ms(50); // debounce

        // Determine which key(s) are pressed:
        //   SW_SEL  -> 1
        //   SW_TRIG -> 2
        //   Both    -> 3
        switch ( input(SW_SEL) + input(SW_TRIG)*2 ) {

            case 1:
                // Measure + show results
                countber();
                show_ber();
                break;

            case 2:
                // Change measurement length (TotalBits)
                setsetting();
                break;

            case 3:
                // Save settings to EEPROM
                printf(lcd_putc, "\fsave settings...\n%s", __DATE__);

                write_eeprom(0, ClockNeg);
                write_eeprom(1, DataNeg);
                write_eeprom(2, TBI);
                write_eeprom(3, ThresError);

                delay_ms(100);
                break;

            default:
                break;
        }
    }
}
