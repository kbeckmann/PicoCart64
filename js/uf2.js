/*
 * https://github.com/wokwi/uf2/blob/master/src/uf2.ts
 * UF2 (USB Flashing Format) JavaScript Library
 *
 * UF2 Format specification: https://github.com/microsoft/uf2
 *
 * Copyright (C) 2021, Uri Shaked.
 * Released under the terms of the MIT License.
 */

const magicValues = [
  { offset: 0, value: 0x0a324655 },
  { offset: 4, value: 0x9e5d5157 },
  { offset: 508, value: 0x0ab16f30 },
];

const maxPayloadSize = 476;

const UF2Flags = {
  notMainFlash: 0x00000001,
  fileContainer: 0x00001000,
  familyIDPresent: 0x00002000,
  md5ChecksumPresent: 0x00004000,
  extensionTagsPresent: 0x00008000,
};

const familyMap = {
  'Microchip (Atmel) SAMD21': 0x68ed2b88,
  'Microchip (Atmel) SAML21': 0x1851780a,
  'Microchip (Atmel) SAMD51': 0x55114460,
  'Nordic NRF52840': 0xada52840,
  'ST STM32F0xx': 0x647824b6,
  'ST STM32F103': 0x5ee21072,
  'ST STM32F2xx': 0x5d1a0a2e,
  'ST STM32F3xx': 0x6b846188,
  'ST STM32F401': 0x57755a57,
  'ST STM32F407': 0x6d0922fa,
  'ST STM32F407VG': 0x8fb060fe,
  'ST STM32F7xx': 0x53b80f00,
  'ST STM32G0xx': 0x300f5633,
  'ST STM32G4xx': 0x4c71240a,
  'ST STM32H7xx': 0x6db66082,
  'ST STM32L0xx': 0x202e3a91,
  'ST STM32L1xx': 0x1e1f432d,
  'ST STM32L4xx': 0x00ff6919,
  'ST STM32L5xx': 0x04240bdf,
  'ST STM32WBxx': 0x70d16653,
  'ST STM32WLxx': 0x21460ff0,
  'Microchip (Atmel) ATmega32': 0x16573617,
  'Cypress FX2': 0x5a18069b,
  'ESP8266': 0x7eab61ed,
  'ESP32': 0x1c5f21b0,
  'ESP32-S2': 0xbfdd4eee,
  'ESP32-C3': 0xd42ba06c,
  'ESP32-S3': 0xc47e5767,
  'NXP i.MX RT10XX': 0x4fb2d5bd,
  'NXP LPC55xx': 0x2abc77ec,
  'GD32F350': 0x31d228c6,
  'Raspberry Pi RP2040': 0xe48bff56,
};


// interface UF2BlockData {
//   /** See UF2Flags for possible flag values. */
//   flags: number;

//   /** Address in flash where the data should be written */
//   flashAddress: number;

//   /** The payload usually contains 256 bytes, but can be up to 476 bytes */
//   payload: Uint8Array;

//   /** Sequential block number; starts at 0 */
//   blockNumber: number;

//   /** Total number of blocks in file */
//   totalBlocks: number;

//   /**
//    * Board family ID, file size, or zero (depending on Flags)
//    */
//   boardFamily: number;
// }

class UF2EncodeError extends Error {}

function encodeBlock(
  blockData,
  target,
  targetOffset
) {
  if (target.length < targetOffset + 512) {
    throw new UF2EncodeError(`Can't encode block: target array is too small`);
  }
  if (blockData.payload.length > maxPayloadSize) {
    throw new UF2EncodeError(`Block payload too big; must be ${maxPayloadSize} bytes or less.`);
  }

  target.fill(0, targetOffset, targetOffset + 512);
  const dataView = new DataView(target.buffer, targetOffset);
  for (let { offset, value } of magicValues) {
    dataView.setUint32(offset, value, true);
  }
  dataView.setUint32(8, blockData.flags, true);
  dataView.setUint32(12, blockData.flashAddress, true);
  dataView.setUint32(16, blockData.payload.length, true);
  dataView.setUint32(20, blockData.blockNumber, true);
  dataView.setUint32(24, blockData.totalBlocks, true);
  dataView.setUint32(28, blockData.boardFamily, true);
  target.set(blockData.payload, targetOffset + 32);
  return target;
}
