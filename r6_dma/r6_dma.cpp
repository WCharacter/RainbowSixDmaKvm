#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <thread>
#include <random>
#include <chrono>
#include "../vmread/hlapi/hlapi.h"
#include "offsets.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

__attribute__((constructor)) static void init()
{
	FILE *out = stdout;
	pid_t pid = 0;
#if (LMODE() != MODE_EXTERNAL())
	pid = getpid();
#endif
	fprintf(out, "Using Mode: %s\n", TOSTRING(LMODE));

	try
	{
		WinContext ctx(pid);
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
						fprintf(out, "\nR6 found %lx:\t%s\n", i.proc.pid, i.proc.name);
						fprintf(out, "\tBase:\t%lx\tMagic:\t%hx (valid: %hhx)\n", peb.ImageBaseAddress, magic, (char)(magic == IMAGE_DOS_SIGNATURE));
						fprintf(out, "Press any key to write memory...\n");
						getchar();  

						auto localplayer = i.Read<uint64_t>(g_Base + PROFILE_MANAGER_OFFSET);
						localplayer = i.Read<uint64_t>(localplayer + 0x68);
						localplayer = i.Read<uint64_t>(localplayer + 0x0);
						localplayer = i.Read<uint64_t>(localplayer + 0x28) + 0xebab0991057478ed;
						fprintf(out, "Localplayer: 0x%lx\n", localplayer);

						auto fov_manager = i.Read<uint64_t>(g_Base + FOV_MANAGER_OFFSET);
						fov_manager = i.Read<uint64_t>(fov_manager + 0xE8);
						fov_manager = i.Read<uint64_t>(fov_manager + 0x88B932A0D99755B8);

						auto fov = i.Read<uint64_t>(g_Base + FOV_MANAGER_OFFSET);
						fov = i.Read<uint64_t>(fov + 0x60) + 0xe658f449242c196;
						auto playerfov = i.Read<uint64_t>(fov + 0x0) + 0x38;
						auto third_person = i.Read<uint64_t>(fov + 0x0) + 0x48;

						auto third_person_val = i.Read<float>(third_person);
						fprintf(out, "Third person: %f\n", third_person_val);
						auto fov_val = i.Read<float>(playerfov);
						fprintf(out, "Player fov: %f\n", fov_val);

						//i.Write<float>(third_person, 0.05); // third person

						//i.Write<float>(playerfov, 1.6f); // player fov ~1.2f-2.3f

						fprintf(out, "Fov manager: %lu\n", fov_manager);
						i.Write<float>(fov_manager + 0xE34, 0.f); // no recoil	

						//i.Write<uint8_t>(g_Base + 0x2DA5D35 + 0x6, 0x1); // run and shoot	
						//i.Write<uint8_t>(g_Base + 0x14765E1 + 0x6 , 0x1); // run and shoot	
						
						auto player = i.Read<uint64_t>(localplayer + 0x30);
						player = i.Read<uint64_t>(player + 0x31);
						auto noflash = i.Read<uint64_t>(player + 0x28);
						i.Write<uint8_t>(noflash + 0x40, 0); // noflash

						auto silent_aim = i.Read<uint64_t>(localplayer + 0x90);
						silent_aim = i.Read<uint64_t>(silent_aim + 0xc8);
						i.Write<uint8_t>(silent_aim + 0x384, 0); //silent aim

						auto weapon = i.Read<uint64_t>(localplayer + 0x90);
						weapon = i.Read<uint64_t>(weapon + 0xc8);
						auto firing_mode_val = i.Read<uint32_t>(weapon + 0x118);
						fprintf(out, "Firing mode: %u\n", firing_mode_val);
						i.Write<uint32_t>(weapon + 0x118, 0); //firing mode 0 - auto 3 - single 2 -  burst

						auto weapon2 = i.Read<uint64_t>(weapon + 0x290) - 0x2b306cb952f73b96;
						i.Write<float>(weapon2 + 0x80, 0.f); // no spread
						
						char shellcode[] = {(char)0xE9, (char)0x92, (char)0xFE, (char)0xFF, (char)0xFF}; 
						i.WriteMem<char>(g_Base + 0x5B61E9, shellcode, sizeof(shellcode));	//cav esp
						fprintf(out, "Cav esp active\n");
						not_found = false;
						break;
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	catch (VMException &e)
	{
		fprintf(out, "Initialization error: %d\n", e.value);
	}

	fclose(out);
}

int main()
{
	return 0;
}
