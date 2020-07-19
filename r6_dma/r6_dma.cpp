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
					
						getchar();  

						auto fov_manager = i.Read<uint64_t>(g_Base + FOV_MANAGER_OFFSET);
						fov_manager = i.Read<uint64_t>(fov_manager + 0xE8);
						fov_manager = i.Read<uint64_t>(fov_manager + 0x88B932A0D99755B8);
	
						fprintf(out, "No recoil: %lu\n", fov_manager);
						i.Write<float>(fov_manager + 0xE34, 0.f); // no recoil	
						//i.Write<uint8_t>(g_Base + 0x2DA5D35 + 0x6, 0x1); // run and shoot	
						//i.Write<uint8_t>(g_Base + 0x14765E1 + 0x6 , 0x1); // run and shoot			

						fprintf(out, "No recoil: %lu\n", fov_manager);
						char shellcode[] = {(char)0xE9, (char)0x92, (char)0xFE, (char)0xFF, (char)0xFF}; 
						i.WriteMem<char>(g_Base + 0x5B61E9, shellcode, sizeof(shellcode));	//cav esp
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
