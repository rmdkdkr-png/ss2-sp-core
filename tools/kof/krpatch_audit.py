#!/usr/bin/env python3
"""KrPatch 릴리즈 전수 감사 — **추측하지 말고 열거해서** 자산과 본문을 대조한다.

왜 만드나: 월화를 감사하면서 태그를 **찍어서 조회**했다(`lastblade-v2.2` 등).
그래서 실제로 배포돼 있던 `lastblade-v0.22` 를 통째로 못 봤고,
로컬 파일이 **날짜가 더 최신인데도** 「옛 초안」이라 단정했다.
목록을 받아 보면 1초에 알 수 있는 것이었다.

검사 항목 (릴리즈마다)
  * 자산 IPS 를 **실제로 내려받아** sha256 을 잰다
  * 본문에 그 해시가 적혀 있는가 (RELEASE_RULES: IPS 해시를 맨 앞에)
  * 본문의 sha256 들 중 자산 해시와 맞는 것이 있는가
  * 자산 이름이 `<접두>_Korean_v<판>.ips` 규칙에 맞는가 (판 토큰에 밑줄 금지)
  * 스크린샷이 있는가 (자산 이미지 또는 본문 이미지 참조)

사용: krpatch_audit.py
"""
import hashlib
import re
import sys
import urllib.request

sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')
import publish_kof as pk

REPO = 'rmdkdkr-png/KrPatch'
# 자동 색인 대상: `<접두>_Korean_v<판>.ips` — 판 토큰은 숫자·영문·점만.
# ⚠ `_allcards` · `_to_v0.2` · `_JUE` 같은 **밑줄 접미사는 규칙이 일부러 허용**한다
#   (파생판 — 자동 색인이 안 집고 사람만 받는다). 그걸 「이름 규칙 어긋남」으로 찍으면
#   본부에 잡음만 보내게 된다. 실제로 한 번 그렇게 찍을 뻔했다.
NAMERE = re.compile(r'^([A-Za-z0-9]+(?:_[A-Za-z0-9]+)*)_Korean_v([0-9A-Za-z.]+)\.ips$')
DERIVRE = re.compile(r'^([A-Za-z0-9]+(?:_[A-Za-z0-9]+)*)_Korean_v([0-9A-Za-z.]+)_[A-Za-z0-9_.]+\.ips$')


def name_ok(n):
    """(규칙적합, 파생판인가) — 파생판은 색인 제외가 **의도된 것**이다."""
    if NAMERE.match(n):
        return True, False
    if DERIVRE.match(n):
        return True, True
    return False, False


def main():
    tok = pk.token()
    rels = pk.call('GET', '%s/repos/%s/releases?per_page=100' % (pk.API, REPO), tok)
    print('릴리즈 %d개\n' % len(rels))
    print('%-18s %-30s %-9s %-7s %-7s %s'
          % ('태그', '자산 IPS', '해시일치', '이름규칙', '스샷', '비고'))
    bad = []
    for r in sorted(rels, key=lambda x: x['tag_name']):
        tag = r['tag_name']
        body = r.get('body') or ''
        ips = [a for a in r['assets'] if a['name'].lower().endswith('.ips')]
        imgs = [a for a in r['assets']
                if a['name'].lower().endswith(('.png', '.jpg', '.gif'))]
        has_img = bool(imgs) or ('<img' in body) or ('![' in body)
        if not ips:
            print('%-18s %-30s %-9s %-7s %-7s %s'
                  % (tag, '(없음)', '-', '-', 'O' if has_img else '★없음',
                     '앱 배포 태그' if tag in ('app', 'pocketcore') else ''))
            continue
        for a in ips:
            d = urllib.request.urlopen(a['browser_download_url']).read()
            h = hashlib.sha256(d).hexdigest()
            inbody = h in body
            ok, deriv = name_ok(a['name'])
            note = []
            if not inbody and not deriv:
                hs = re.findall(r'\b[0-9a-f]{64}\b', body)
                note.append('본문에 이 IPS 해시가 없다 (본문 sha256 %d개)' % len(hs))
                bad.append((tag, a['name'], 'IPS 해시 누락'))
            if not ok:
                note.append('이름 규칙 어긋남')
                bad.append((tag, a['name'], '이름'))
            if deriv:
                note.append('파생판(색인 제외 — 의도된 것)')
            if not has_img:
                note.append('스샷 없음')
            print('%-18s %-30s %-9s %-7s %-7s %s'
                  % (tag, a['name'], 'O' if (inbody or deriv) else '★X',
                     'O' if ok else '★X', 'O' if has_img else '★X',
                     ' / '.join(note)))
    print()
    if bad:
        print('★ 손봐야 할 것 %d건:' % len(bad))
        for t, n, why in bad:
            print("   %-18s %-34s %s" % (t, n, why))
    else:
        print('전부 통과 — 자산 해시가 본문에 있고 이름 규칙도 맞는다.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
