//
// Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/* Using VCAST_CUSTOM_STD_OUTPUT_INCLUDE to add code into c_cover_io.c */
/* Note: This code is #included into the middle of a function in c_cover_io.c, so
 * don't do things like #include other headers, etc. */

 /* This code should take the NULL-terminated char *S and write it out somewhere
  * e.g. a serial port. */


// void VCAST_WRITE_TO_INST_FILE_COVERAGE (const char *S, int flush) {
  extern UINTN EFIAPI SerialPortWrite (IN UINT8 *Buffer,  IN UINTN NumberOfBytes);
  UINTN NumberOfBytes = 0;
  while (S[NumberOfBytes] != '\0') {
    NumberOfBytes++;
  }

  SerialPortWrite ((UINT8 *)"\r\n", 2);
  SerialPortWrite ((UINT8 *)S, NumberOfBytes);
  SerialPortWrite ((UINT8 *)"\r\n", 2);
//}

