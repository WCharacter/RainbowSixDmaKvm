#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <random>
#include <thread>
#include <chrono>
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

struct R6Data
{
	uint64_t base;
	uint64_t localplayer;
	uint64_t fovmanager;
	uint64_t currweapon;
	uint64_t weaponinfo;
	uint64_t glowmanager;
};

uint64_t get_base(WinProcess &proc)
{
	PEB peb = proc.GetPeb();
	return peb.ImageBaseAddress;
}

void read_data(WinProcess &proc, R6Data &data, bool init = true)
{
	if (init)
	{
		data.base = get_base(proc);
	}

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
	data.currweapon = weapon;

	auto weapon2 = proc.Read<uint64_t>(data.currweapon + 0x290) - 0x2b306cb952f73b96;
	data.weaponinfo = weapon2;

	auto glow = proc.Read<uint64_t>(data.base + GLOW_MANAGER_OFFSET);
	data.glowmanager = glow;
}

void enable_esp(WinProcess &proc, const R6Data &data)
{
	char shellcode[] = {(char)0xE9, (char)0x92, (char)0xFE, (char)0xFF, (char)0xFF};
	proc.WriteMem<char>(data.base + 0x5B61E9, shellcode, sizeof(shellcode)); //cav esp
	printf("Cav esp active\n");
}

void enable_no_recoil(WinProcess &proc, const R6Data &data)
{
	proc.Write<float>(data.fovmanager + 0xE34, 0.f); 
	proc.Write<float>(data.weaponinfo + 0x198, 0.f); //rec b
	proc.Write<float>(data.weaponinfo + 0x18c, 0.f); //rec v
	proc.Write<float>(data.weaponinfo + 0x17c, 0.f); //rec h
	printf("No recoil active\n");
}

void enable_no_spread(WinProcess &proc, const R6Data &data)
{
	proc.Write<float>(data.weaponinfo + 0x80, 0.f); // no spread
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
	proc.Write<uint32_t>(data.currweapon + 0x118, (uint32_t)mode); //firing mode 0 - auto 3 - single 2 -  burst
	printf("Fire mode: %s\n", (mode == FiringMode::AUTO ? "auto" : mode == FiringMode::BURST ? "burst" : "single"));
}

void enable_glow(WinProcess &proc, const R6Data &data)
{	
	auto chain = proc.Read<uint64_t>(data.glowmanager  + 0xb8);
	
	proc.Write<float>(chain + 0xd0, -10.f);  //r
	proc.Write<float>(chain + 0xd4, 0.f);    //g
	proc.Write<float>(chain + 0xd8, -10.f);  //b
	proc.Write<float>(chain + 0x110, 1.f);   //a
	proc.Write<float>(chain + 0x114, 1.f); 
	proc.Write<float>(chain + 0x118, 0.f); //glow 
	proc.Write<float>(chain + 0x11c, 5.f); //opacity
	printf("Glow active\n");
}

void write_loop(WinProcess &proc, R6Data &data)
{
	printf("Write thread started\n");
	while (run_cheat)
	{
		read_data(proc, data, data.base == 0x0 || data.fovmanager == 0x0 || data.currweapon == 0);
		float spread_val = proc.Read<float>(data.weaponinfo + 0x80);
		
		if (spread_val != 0.f)
		{
			enable_esp(proc, data);
			enable_no_recoil(proc, data);
			enable_no_spread(proc, data);		
			enable_no_flash(proc, data);
			enable_no_aim_animation(proc, data);
			enable_glow(proc, data);
			set_firing_mode(proc, data, FiringMode::AUTO);
			printf("Data updated\n\n");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	printf("Exiting...\n");
}

__attribute__((constructor)) static void init()
{
	pid_t pid = 0;
#if (LMODE() != MODE_EXTERNAL())
	pid = getpid();
#endif
	printf("Using Mode: %s\n", TOSTRING(LMODE));

	try
	{
		R6Data data;
		bool not_found = true;
		WinContext write_ctx(pid);
		WinContext check_ctx(pid);
		while (not_found)
		{
			write_ctx.processList.Refresh();
			for (auto &i : write_ctx.processList)
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
						printf("Press enter to start\n");
						getchar();
						read_data(i, data);

						printf("Base: 0x%lx\nFOV Manager: 0x%lx\nLocal player: 0x%lx\nWeapon: 0x%lx\nGlow manager: 0x%lx\n", data.base, data.fovmanager, data.localplayer, data.currweapon, data.glowmanager);

						enable_esp(i, data);
						enable_no_recoil(i, data);
						enable_no_spread(i, data);
						enable_no_flash(i, data);
						enable_no_aim_animation(i, data);
						enable_glow(i, data);
						//enable_run_and_shoot(i, data);

						set_firing_mode(i, data, FiringMode::AUTO);

						// float fov;
						// fprintf(out, "Enter fov: ");
						// scanf("%f", &fov);
						// fprintf(out, "\n");
						// set_fov(i, data, fov);

						//checking if weapon is updated
						std::thread write_thread(write_loop, std::ref(i), std::ref(data));
						write_thread.detach();

						not_found = false;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		while (run_cheat)
		{
			bool flag = false;
			check_ctx.processList.Refresh();
			for (auto &i : check_ctx.processList)
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
				run_cheat = false;

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
	catch (VMException &e)
	{
		printf("Initialization error: %d\n", e.value);
	}
}

int main()
{
	return 0;
}
