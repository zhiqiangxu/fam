os = linux osx
.PHONY: $(os)

$(os):
	make -C "sysdep/$@" $(target)

