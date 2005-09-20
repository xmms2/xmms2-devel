import os
import sha

def gitsha(path):
    h = sha.sha()
    data = file(path).read()
    h.update("blob %d\0" % len(data))
    h.update(data)
    return h.hexdigest()

def git_info():
    commithash = file('.git/HEAD').read().strip()
    os.system('git-update-cache --refresh >/dev/null')
    changed = bool(os.popen('git-diff-cache -r HEAD').read())
    return commithash, changed

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

