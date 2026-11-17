import sys, time, collections
sys.path.insert(0,'/Users/spot/Code/UP-Term/vtcon/tools/rig')
import ami, condev_rig as c, rig
import patina_theme_save_rig as t
LOG=open(sys.argv[1],'a')
def P(s): print(s,flush=True); LOG.write(s+'\n'); LOG.flush()
def hx(p): return '%02X%02X%02X'%tuple(p)
def sample(label):
    d=c.run('VTCX:Apps/bin/dripens Workbench')[1]
    f={l.split(':')[0]:l.split(':',1)[1].split() for l in d.splitlines() if ':' in l}
    pal=f['pal']
    env=c.run('List ENV:PATINA_#? NOHEAD')[1]
    names=sorted(l.split()[0] for l in env.splitlines() if l.strip().startswith('PATINA_'))
    P('=== %s'%label)
    for k in ('scr','pens','rgb','cols'): P('  %s: %s'%(k,' '.join(f[k])))
    P('  pal0-7: %s'%' '.join(pal[:8])); P('  pal248-255: %s'%' '.join(pal[-8:]) + '  (npal=%d)'%len(pal))
    P('  env: '+' '.join('%s=%s'%(k,t.env(k) or '-') for k in names))
    w,h,rows=ami.grab()
    px=lambda x,y: tuple(rows[y][x*3:x*3+3])
    P('  screen %dx%d'%(w,h))
    col=lambda p: ('pal%d'%pal.index(hx(p))) if hx(p) in pal else 'nopal'
    # column runs at several x
    for x in (w//2, w//2+97, w-300):
        runs=[]; 
        for y in range(0,48):
            p=px(x,y)
            if runs and runs[-1][0]==p: runs[-1][2]=y
            else: runs.append([p,y,y])
        P('  x=%d rows: '%x+' '.join('%d-%d:%s(%s)'%(r[1],r[2],hx(r[0]),col(r[0])) for r in runs))
    cnt=collections.Counter(px(x,y) for y in range(0,16) for x in range(0,260))
    P('  x0-259,y0-15 colours: '+' '.join('%s(%s)=%d'%(hx(p),col(p),n) for p,n in cnt.most_common(6)))
    return pal
sample('BEFORE')
allowed={w['title'] or '' for s in t.g.screens() for w in s['windows']}
for n in map(int,sys.argv[2:]):
    P('### select %d'%n)
    ok,detail,_=t.one_select(n,allowed)
    P('### %s %s | theme=%s'%('ok' if ok else 'FAIL',detail[:150],t.theme_line('ENV:Patina/prefs')))
    time.sleep(3); sample('after select %d'%n)
