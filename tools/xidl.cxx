/* -------------------------------------------

	Copyright ZKA Web Services Co

------------------------------------------- */

/// @file Linker.cxx
/// @brief ZKA Linker for AE objects.

extern "C" int ZKAXIDLMain(int argc, char const* argv[]);

int main(int argc, char const* argv[])
{
	if (argc < 1)
	{
		return 1;
	}

	return ZKAXIDLMain(argc, argv);
}
