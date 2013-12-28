git grep -l -E "Copyright.+XMMS2 Team" | \
    xargs -I {} sed -i -r -e"s/Copyright \(C\) ([0-9]{4})-?[0-9]* XMMS2 Team/Copyright (C) \1-`date +%Y` XMMS2 Team/g" {}
