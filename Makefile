os = linux
.PHONY: $(os)

$(os):
	make -C "sysdep/$@" $(target)

