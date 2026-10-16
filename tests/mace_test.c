/* Golden PCM hashes from FFmpeg 6.1.1 decoding deterministic AIFF-C inputs. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/include/rsys/mace.h"

static uint32_t
checksum (const uint8_t *p, size_t n)
{
  uint32_t hash = 2166136261U;
  while (n--)
    hash = (hash ^ *p++) * 16777619U;
  return hash;
}

int
main (void)
{
  uint8_t input[2048], stereo[4096], output[12290], mono[12288];
  uint32_t seed = 0x12345678;
  unsigned i, mace3;
  for (i = 0; i < sizeof input; ++i)
    {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      input[i] = seed;
    }
  for (mace3 = 0; mace3 <= 1; ++mace3)
    {
      size_t n = sizeof input * (mace3 ? 3 : 6);
      unsigned packet_bytes = mace3 ? 2 : 1;
      memset (output, 0x5A, sizeof output);
      assert (ROMlib_mace_decode (input, sizeof input, 1, mace3, output + 1, n));
      assert (output[0] == 0x5A && output[n + 1] == 0x5A);
      assert (checksum (output + 1, n) == (mace3 ? 0x5AF088ABU : 0x13EBA7F1U));
      memcpy (mono, output + 1, n);
      for (i = 0; i < sizeof input / packet_bytes; ++i)
        {
          memcpy (stereo + i * packet_bytes * 2, input + i * packet_bytes, packet_bytes);
          memcpy (stereo + (i * 2 + 1) * packet_bytes, input + i * packet_bytes, packet_bytes);
        }
      assert (ROMlib_mace_decode (stereo, sizeof stereo, 2, mace3, output, n));
      assert (!memcmp (output, mono, n));
      assert (!ROMlib_mace_decode (input, sizeof input, 1, mace3, output, n - 1));
      assert (!ROMlib_mace_decode (input, sizeof input, 3, mace3, output, n));
    }
  assert (!ROMlib_mace_decode (input, sizeof input - 1, 1, 1, output, sizeof output));
  puts ("MACE 3:1/6:1 reference PCM, stereo downmix and bounds tests passed");
  return 0;
}
