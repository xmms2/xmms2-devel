VERSION=$(grep "^BASEVERSION" wscript | sed -r 's/BASEVERSION=["]([^"]+)["]/\1/')
TAG=$(echo $VERSION | cut -f1 -d ' ')

export GIT_AUTHOR_NAME="XMMS2 Release"
export GIT_AUTHOR_EMAIL="release@xmms2.org"

echo -e "New version: '$VERSION' (tag: $TAG)\n"
git diff | cat

echo DRY RUN: git commit -a -m "'RELEASE: $VERSION'"
echo DRY RUN: git tag -u 7A8057EF -s -m "'$VERSION'" $TAG
