import os
try: from hashlib import sha1 as sha
except ImportError: from sha import sha

def gitsha(path):
    h = sha()
    data = file(path, 'rb').read()
    h.update("blob %d\0" % len(data))
    h.update(data)
    return h.hexdigest()

def git_info():
    commithash = os.popen('git rev-parse --verify HEAD 2>/dev/null').read().strip()
    if not commithash:
        raise ValueError("Couldn't get hash")
    if os.getuid() == os.stat(".git/index").st_uid:
        os.system('git update-index --refresh >/dev/null')
    else:
        print("NOT updating git cache, local changes might not be detected")
    changed = bool(os.popen('git diff-index -r HEAD').read())
    return commithash[:8], changed

def snapshot_info():
    info = file('commithash').read().split('\n')

    commithash = info[0]

    changed = False
    for line in [a for a in info[2:] if a]:
        [mode, tag, sha, path] = line.split(None, 4)
        if tag != 'blob':
            continue
        if gitsha(path) != sha:
            changed = True
            break
    return commithash, changed

def get_info():
    try:
        return git_info()
    except:
        try:
            return snapshot_info()
        except:
            return 'Unknown', False

def get_info_str():
    commithash, changed = get_info()
    if changed:
        changed = " + local changes"
    else:
        changed = ""

    return "%s%s" % (commithash, changed)

submodule_status = {'-':'missing', '+':'outdated', ' ':'uptodate'}
def git_submodules():
    submodules = {}
    for l in os.popen('git submodule').read().split('\n'):
        status = submodule_status.get(l and l[0] or "", 'uptodate')
        l = l[1:].strip()
        if not l:
            continue
        commithash, folder = l.strip().split()[:2]
        submodules[folder] = (status, commithash[:8])
    return submodules

def get_submodules():
    try:
        return git_submodules()
    except:
        return {}
