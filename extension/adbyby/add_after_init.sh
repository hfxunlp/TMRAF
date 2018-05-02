export adbyby_on=True

if [[ $adbyby_on == "True" && -d "/etc/storage/adbyby" && -f "/etc/storage/adbyby/post_init.sh" ]]; then
	chmod a+x /etc/storage/adbyby/post_init.sh
	/bin/sh /etc/storage/adbyby/post_init.sh > /dev/null 2>&1 &
fi
