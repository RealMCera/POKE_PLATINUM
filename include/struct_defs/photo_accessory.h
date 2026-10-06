#ifndef POKEPLATINUM_STRUCT_PHOTO_ACCESSORY_H
#define POKEPLATINUM_STRUCT_PHOTO_ACCESSORY_H

// An accessory placed on a photo, with the position and draw priority it was
// placed at.
typedef struct PhotoAccessory {
    u8 accessoryID;
    u8 xPos;
    u8 yPos;
    s8 priority;
} PhotoAccessory;

#endif // POKEPLATINUM_STRUCT_PHOTO_ACCESSORY_H
