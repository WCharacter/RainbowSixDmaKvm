#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <random>
#include <thread>
#include <chrono>
#include <mutex>
#include "../vmread/hlapi/hlapi.h"
#include "offsets.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

enum class FiringMode : uint8_t
{
	AUTO = 0,
	SINGLE = 3,
	BURST = 2
};

static bool run_cheat = true;
std::mutex mtx;

struct R6Data
{
	uint64_t base;
	uint64_t localplayer;
	uint64_t fovmanager;
	uint64_t weapon;
};

uint64_t get_base(WinProcess &proc)
{
	PEB peb = proc.GetPeb();
	return peb.ImageBaseAddress;
}

void read_data(WinProcess &proc, R6Data &data)
{
	data.base = get_base(proc);

	auto localplayer = proc.Read<uint64_t>(data.base + PROFILE_MANAGER_OFFSET);
	localplayer = proc.Read<uint64_t>(localplayer + 0x68);
	localplayer = proc.Read<uint64_t>(localplayer + 0x0);
	localplayer = proc.Read<uint64_t>(localplayer + 0x28) + 0xebab0991057478ed;
	data.localplayer = localplayer;

	auto fov_manager = proc.Read<uint64_t>(data.base + FOV_MANAGER_OFFSET);
	fov_manager = proc.Read<uint64_t>(fov_manager + 0xE8);
	fov_manager = proc.Read<uint64_t>(fov_manager + 0x88B932A0D99755B8);
	data.fovmanager = fov_manager;

	auto weapon = proc.Read<uint64_t>(localplayer + 0x90);
	weapon = proc.Read<uint64_t>(weapon + 0xc8);

	data.weapon = weapon;
}

void enable_esp(WinProcess &proc, const R6Data &data)
{
	char shellcode[] = {(char)0xE9, (char)0x92, (char)0xFE, (char)0xFF, (char)0xFF};
	proc.WriteMem<char>(data.base + 0x5B61E9, shellcode, sizeof(shellcode)); //cav esp
	printf("Cav esp active\n");
}

void enable_no_recoil(WinProcess &proc, const R6Data &data)
{
	proc.Write<float>(data.fovmanager + 0xE34, 0.f); // no recoil
	printf("No recoil active\n");
}

void enable_no_spread(WinProcess &proc, const R6Data &data)
{
	auto weapon2 = proc.Read<uint64_t>(data.weapon + 0x290) - 0x2b306cb952f73b96;
	proc.Write<float>(weapon2 + 0x80, 0.f); // no spread
	printf("No spread active\n");
}

void enable_run_and_shoot(WinProcess &proc, const R6Data &data)
{
	proc.Write<uint8_t>(data.base + 0x2DA5D35 + 0x6, 0x1); // run and shoot
	proc.Write<uint8_t>(data.base + 0x14765E1 + 0x6, 0x1); // run and shoot
	printf("Run and shoot active\n");
}

void enable_no_flash(WinProcess &proc, const R6Data &data)
{
	auto player = proc.Read<uint64_t>(data.localplayer + 0x30);
	player = proc.Read<uint64_t>(player + 0x31);
	auto noflash = proc.Read<uint64_t>(player + 0x28);
	proc.Write<uint8_t>(noflash + 0x40, 0); // noflash
	printf("Noflash active\n");
}

void enable_no_aim_animation(WinProcess &proc, const R6Data &data)
{
	auto no_anim = proc.Read<uint64_t>(data.localplayer + 0x90);
	no_anim = proc.Read<uint64_t>(no_anim + 0xc8);
	proc.Write<uint8_t>(no_anim + 0x384, 0); 
	printf("No aim animation active\n");
}

void set_fov(WinProcess &proc, const R6Data &data, float fov_val)
{
	auto fov = proc.Read<uint64_t>(data.base + FOV_MANAGER_OFFSET);
	fov = proc.Read<uint64_t>(fov + 0x60) + 0xe658f449242c196;
	auto playerfov = proc.Read<uint64_t>(fov + 0x0) + 0x38;
	proc.Write<float>(playerfov, fov_val); // player fov ~1.2f-2.3f
	printf("Player fov changed to %f\n", fov_val);
}

void set_firing_mode(WinProcess &proc, const R6Data &data, FiringMode mode)
{
	proc.Write<uint32_t>(data.weapon + 0x118, (uint32_t)mode); //firing mode 0 - auto 3 - single 2 -  burst
}

void write_loop()
{
	pid_t pid = 0;
#if (LMODE() != MODE_EXTERNAL())
	pid = getpid();
#endif
	printf("Write thread started\n");

	try
	{
		R6Data data;
		mtx.lock();
		WinContext ctx(pid);
		mtx.unlock();
		while (run_cheat)
		{
			bool not_found = true;

			while (not_found)
			{
				ctx.processList.Refresh();

				for (auto &i : ctx.processList)
				{
					if (!strcasecmp("RainbowSix.exe", i.proc.name))
					{
						short magic = i.Read<short>(i.GetPeb().ImageBaseAddress);
						auto peb = i.GetPeb();
						auto g_Base = peb.ImageBaseAddress;
						if (g_Base != 0)
						{
							printf("\nR6 found %lx:\t%s\n", i.proc.pid, i.proc.name);
							printf("\tBase:\t%lx\tMagic:\t%hx (valid: %hhx)\n", peb.ImageBaseAddress, magic, (char)(magic == IMAGE_DOS_SIGNATURE));

							read_data(i, data);

							printf("Base: 0x%lx\nFOV Manager: 0x%lx\nLocal player: 0x%lx\nWeapon: 0x%lx\n", data.base, data.fovmanager, data.localplayer, data.weapon);

							enable_esp(i, data);
							enable_no_recoil(i, data);
							enable_no_flash(i, data);
							enable_no_spread(i, data);
							enable_no_aim_animation(i, data);
							//enable_run_and_shoot(i, data);

							set_firing_mode(i, data, FiringMode::AUTO);

							// float fov;
							// fprintf(out, "Enter fov: ");
							// scanf("%f", &fov);
							// fprintf(out, "\n");
							// set_fov(i, data, fov);

							//checking if weapon is updated
							while (run_cheat)
							{
								R6Data new_data;
								read_data(i, new_data);
								auto weapon2 = i.Read<uint64_t>(new_data.weapon + 0x290) - 0x2b306cb952f73b96;
								float spread_val = i.Read<float>(weapon2 + 0x80);
								if (new_data.weapon != data.weapon && spread_val != 0.f)
								{
									data = new_data;
									//this things should be updated every round
									enable_no_spread(i, data);							
									set_firing_mode(i, data, FiringMode::AUTO);
									printf("Data updated\n");
								}
								std::this_thread::sleep_for(std::chrono::milliseconds(10));
							}
							not_found = false;
							break;
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
	catch (VMException &e)
	{
		printf("Initialization error: %d\n", e.value);
	}
	printf("Exiting...\n");
}

void alive_check()
{
	pid_t pid = 0;
#if (LMODE() != MODE_EXTERNAL())
	pid = getpid();
#endif
	printf("Check thread started\n");
	mtx.lock();
	WinContext ctx(pid);
	mtx.unlock();
	while (run_cheat)
	{		
		bool flag = false;
		ctx.processList.Refresh();
		for (auto &i : ctx.processList)
		{
			if (!strcasecmp("RainbowSix.exe", i.proc.name))
			{
				auto peb = i.GetPeb();
				auto g_Base = peb.ImageBaseAddress;
				if (g_Base != 0)
				{
					flag = true;
					break;
				}
			}
		}
		if (!flag)
		{
			run_cheat = false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

__attribute__((constructor)) static void init()
{
	printf("Using Mode: %s\n", TOSTRING(LMODE));
	std::thread check_thread(alive_check);
	std::thread write_thread(write_loop);
	write_thread.join();
	check_thread.join();
}

int main()
{
	return 0;
}
