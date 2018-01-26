MODULES = tpvm player rsa
IP = 192.168.56.101

.PHONY: all modules $(MODULES) cscope

all: $(MODULES) cscope

modules: $(MODULES)

$(MODULES):
	@$(MAKE) -C $@

cscope:
	@cscope -bR

clean:
	@for i in $(MODULES) ; do \
	  $(MAKE) --directory=$$i clean ; \
	done
sync:
	@for i in $(MODULES) videos; do \
		rsync -avz $$i asaf@${IP}:/home/asaf/dev/drm ; \
	done
