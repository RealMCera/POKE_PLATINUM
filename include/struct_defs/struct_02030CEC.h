#ifndef POKEPLATINUM_STRUCT_02030CEC_H
#define POKEPLATINUM_STRUCT_02030CEC_H

// Save-data block backing the Wii message (email) settings. The email string is
// the player's Wii number in the form "w<16 digits>@wii.com"; the remaining
// fields cache the World Exchange trainer profile that is uploaded to the GTS
// when the Wii number is registered.
typedef struct EmailSaveData {
    // Wii number address (up to 50 characters plus a terminator).
    char email[51];
    u8 padding_33[1];
    // Nonzero while the player wants to receive Wii messages. Copied into the
    // World Exchange trainer profile as `emailInitialised`.
    int wiiMessageReception;
    // Random value assigned to the World Exchange trainer profile.
    u16 rngValue;
    // Last four digits of the Wii Registration Code.
    u16 registrationCode;
    // Four-digit Wii number password used to confirm the Wii number.
    u32 password;
} EmailSaveData;

#endif // POKEPLATINUM_STRUCT_02030CEC_H
