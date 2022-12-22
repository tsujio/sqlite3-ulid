PROGRAM = libsqlite-ulid.so

$(PROGRAM): ulid.c
	gcc -I sqlite -fPIC -shared ulid.c -o $(PROGRAM)
	strip $(PROGRAM)
