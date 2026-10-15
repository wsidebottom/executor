/* Exercise the production SDL callback/worker handshake without audio hardware. */
#include "../src/config/sound/sdl/sdl-sound.c"
#undef main
#undef NDEBUG
#include <assert.h>

CPUState cpu_state;
sound_driver_t sound_driver;
boolean_t sound_disabled_p;
uint32 ROMlib_offset, syn68k_address_mask = 0xFFFFFFFF;
unsigned syn68k_host_alias_count;
uint32 syn68k_host_alias_base, syn68k_host_alias_size;
void *syn68k_guest_to_host (uint32 address) { abort (); }
syn68k_addr_t callback_install (callback_handler_t fn, void *arg) { abort (); }
syn68k_addr_t sound_callback (syn68k_addr_t pc, void *arg) { abort (); }
static SDL_sem *callback_done;
static uint8 samples[64];

void
_gui_fatal (const char *file, int line, const char *fn, const char *fmt, ...)
{
  fprintf (stderr, "Audio failure at %s:%d in %s: %s\n", file, line, fn, fmt);
  abort ();
}

static void *
run_callback (void *unused)
{
  sound_sdl_hunger_callback (NULL, samples, sizeof samples);
  SDL_SemPost (callback_done);
  return NULL;
}

static void
wait_for_stream (void)
{
  Uint32 start = SDL_GetTicks ();
  boolean_t ready;
  do
    {
      SDL_LockMutex (audio_mutex);
      ready = sdl_stream != NULL;
      SDL_UnlockMutex (audio_mutex);
      if (ready)
        return;
      SDL_Delay (1);
    }
  while (SDL_GetTicks () - start < 2000);
  assert (!"Audio callback failed to request a buffer");
}

static void
finish_callback (pthread_t thread, uint8 expected)
{
  int i;
  assert (SDL_SemWaitTimeout (callback_done, 2000) == 0);
  pthread_join (thread, NULL);
  for (i = 0; i < sizeof samples; ++i)
    assert (samples[i] == expected);
}

int
main (void)
{
  pthread_t callback_thread;
  int i;
  assert (SDL_Init (SDL_INIT_TIMER) == 0);
  audio_mutex = SDL_CreateMutex ();
  audio_ready = SDL_CreateCond ();
  audio_filled = SDL_CreateCond ();
  callback_done = SDL_CreateSemaphore (0);
  assert (audio_mutex && audio_ready && audio_filled && callback_done);
  assert (sem_init (&semid, 0, 0) == 0);
  num_samples = sizeof samples;
  memset (buf, 0x80, sizeof buf);
  assert (pthread_create (&audio_thread, NULL, loop, NULL) == 0);
  thread_started = TRUE;
  have_sound_p = TRUE;

  for (i = 0; i < 50; ++i)
    {
      struct hunger_info info;
      int j;

      sound_sdl_go (NULL);
      /* Enqueuing many commands must not create duplicate worker wakeups. */
      for (j = 0; j < 20; ++j)
        sound_sdl_go (NULL);
      assert (pthread_create (&callback_thread, NULL, run_callback, NULL) == 0);
      wait_for_stream ();
      sound_sdl_hunger_start (NULL);
      info = sound_sdl_get_hunger_info (NULL);
      memset (info.buf + info.t2 % info.bufsize, 0x40 + i, num_samples);
      sound_sdl_hunger_finish (NULL);
      finish_callback (callback_thread, 0x40 + i);

      /* Stop after the next callback arrives but before another mix. */
      assert (pthread_create (&callback_thread, NULL, run_callback, NULL) == 0);
      wait_for_stream ();
      sound_sdl_stop (NULL);
      finish_callback (callback_thread, 0x80);

      /* Further idle callbacks must return silence without waiting. */
      memset (samples, 0x33, sizeof samples);
      assert (pthread_create (&callback_thread, NULL, run_callback, NULL) == 0);
      finish_callback (callback_thread, 0x80);
    }

  sound_sdl_go (NULL);
  assert (pthread_create (&callback_thread, NULL, run_callback, NULL) == 0);
  wait_for_stream ();
  sound_sdl_stop (NULL);
  finish_callback (callback_thread, 0x80);
  sound_sdl_shutdown (NULL);
  SDL_DestroySemaphore (callback_done);
  SDL_Quit ();
  puts ("SDL sound: playback, stop, idle silence, restart and shutdown passed");
  return 0;
}
