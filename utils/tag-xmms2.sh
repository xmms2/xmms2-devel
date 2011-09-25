VERSION=$(grep "^BASEVERSION" wscript | sed -r 's/BASEVERSION=["]([^"]+)["]/\1/')
TAG=$(echo $VERSION | tr -d ' ')

echo -e "New version: '$VERSION' (tag: $TAG)\n"
git diff

echo -n "Accept? [y/N] "
read ACCEPT

if [ x$ACCEPT = "xY" ] || [ x$ACCEPT = "xy" ]; then
	git commit -a -m "RELEASE: $VERSION"
	git tag -u 7A8057EF -s -m "$VERSION" $TAG
else
	echo "Aborting..."
fi
