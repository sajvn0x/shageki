run:
	@cmake -S . -B build && cd build && ./shageki

fmt:
	@clang-format -i src/*

