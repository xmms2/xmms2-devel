git grep -E "Copyright.+XMMS2 Team" | cut -d ":" -f 1 | xargs -I {} sed -i -r -e"s/Copyright [^0-9]+([0-9]{4})-.+ XMMS2 Team/Copyright (C) \1-`date +%Y` XMMS2 Team/g" {}
