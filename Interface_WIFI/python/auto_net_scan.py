#!/usr/bin/env python3
"""
auto_net_scan.py
自动根据本机所有活动 IPv4 地址枚举子网并扫描每个子网的主机（TCP connect + HTTP probe）。

用法示例：
  python auto_net_scan.py
  python auto_net_scan.py --ports 80,8080,1883 --concurrency 150 --no-ping
  python auto_net_scan.py --allow-large-nets
  python auto_net_scan.py --ifaces "WiFi,ZeroTier One"

注意：
 - 默认对于大于 /24 的网络（例如 /18、/16）会 **缩小为当前主机所在 /24** 以避免过慢扫描。
 - 若你确实需要扫描完整大网，加上 --allow-large-nets 参数。
 - 依赖: aiohttp, netifaces, tabulate (tabulate 可选)
   pip install aiohttp netifaces tabulate
"""

import argparse
import asyncio
import ipaddress
import platform
import time
from typing import List, Dict, Optional, Tuple, Any
import requests

import netifaces
import aiohttp

from tabulate import tabulate

    
import curses

# 默认探测端口与 HTTP path
DEFAULT_PORTS = [80] # , 443, 8080, 8000, 81, 1883, 23
STATUS_PATHS = ["/", "/status", "/status.json", "/json", "/info", "/metrics", "/api/status", "/device", "/esp"]

TCP_TIMEOUT_DEFAULT = 0.8
HTTP_TIMEOUT_DEFAULT = 3.5
CONCURRENCY_DEFAULT = 200
LARGE_NET_THRESHOLD = 20  # 若网络主机数 > 512（例如 /23 或更大），默认缩小到 /24 附近

def is_windows() -> bool:
    return platform.system().lower().startswith("win")

def enumerate_ipv4_networks(iface_filter: Optional[List[str]] = None) -> List[Tuple[str, ipaddress.IPv4Network, str]]:
    """
    返回列表 (iface_name, network (IPv4Network), ip_addr)
    iface_filter: 可传接口名列表，仅包含这些接口
    """
    nets = []
    for iface in netifaces.interfaces():
        if iface_filter and iface not in iface_filter and iface not in [name.strip() for name in iface_filter]:
            continue
        addrs = netifaces.ifaddresses(iface)
        if netifaces.AF_INET not in addrs:
            continue
        for a in addrs[netifaces.AF_INET]:
            ip = a.get("addr")
            netmask = a.get("netmask")
            if not ip or not netmask:
                continue
            if ip.startswith("127."):
                continue
            try:
                net = ipaddress.IPv4Network(f"{ip}/{netmask}", strict=False)
                nets.append((iface, net, ip))
            except Exception:
                continue
    # 去重相同 network（不同别名）并返回
    unique = {}
    for iface, net, ip in nets:
        key = str(net)
        if key not in unique:
            unique[key] = (iface, net, ip)
        else:
            # 保留首个出现的 iface
            pass
    return list(unique.values())

async def icmp_ping(ip: str, count: int = 1, timeout_ms: int = 1000) -> bool:
    """通过系统 ping 命令快速判断是否可达。若你在非常受限的环境可用 --no-ping 跳过此步骤。"""
    if is_windows():
        cmd = f"ping -n {count} -w {timeout_ms} {ip}"
    else:
        cmd = f"ping -c {count} -W {int(timeout_ms/1000)} {ip}"
    proc = await asyncio.create_subprocess_shell(cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    stdout, stderr = await proc.communicate()
    out = (stdout or b"").decode(errors="ignore") + (stderr or b"").decode(errors="ignore")
    if "TTL=" in out.upper() or "bytes from" in out.lower():
        return True
    return False

async def tcp_connect(ip: str, port: int, timeout: float) -> bool:
    try:
        fut = asyncio.open_connection(ip, port)
        reader, writer = await asyncio.wait_for(fut, timeout=timeout)
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:
            pass
        return True
    except Exception:
        return False

async def http_probe(session: aiohttp.ClientSession, ip: str, port: int, http_timeout: float) -> Optional[Dict]:
    scheme = "https" if port == 443 else "http"
    for path in STATUS_PATHS:
        url = f"{scheme}://{ip}:{port}{path}"
        try:
            async with session.get(url, timeout=http_timeout, ssl=False) as resp:
                text = await resp.text()
                try:
                    j = await resp.json()
                except Exception:
                    j = None
                return {"url": url, "status": resp.status, "text": text[:2000], "json": j}
        except Exception:
            continue
    return None

async def probe_host(ip: str, ports: List[int], do_ping: bool, tcp_timeout: float, http_timeout: float, sem: asyncio.Semaphore):
    async with sem:
        result = {"ip": ip, "ping": None, "open_ports": [], "http": None}
        if do_ping:
            try:
                result["ping"] = await icmp_ping(ip, count=1, timeout_ms=int(tcp_timeout*1000))
            except Exception:
                result["ping"] = None
        # 尽管 ping 可能为 False，仍然尝试端口连接（某些设备禁 ICMP）
        tasks = [asyncio.create_task(tcp_connect(ip, p, tcp_timeout)) for p in ports]
        res = await asyncio.gather(*tasks)
        for p, ok in zip(ports, res):
            if ok:
                result["open_ports"].append(p)
        if result["open_ports"]:
            async with aiohttp.ClientSession() as session:
                for p in sorted(result["open_ports"]):
                    probe = await http_probe(session, ip, p, http_timeout)
                    if probe:
                        result["http"] = probe
                        break
        return result

async def scan_network_hosts(network: ipaddress.IPv4Network, ports: List[int], do_ping: bool, tcp_timeout: float, http_timeout: float, concurrency: int):
    hosts = [str(h) for h in network.hosts()]
    sem = asyncio.Semaphore(concurrency)
    tasks = [asyncio.create_task(probe_host(ip, ports, do_ping, tcp_timeout, http_timeout, sem)) for ip in hosts]
    results = []
    start = time.time()
    for t in asyncio.as_completed(tasks):
        r = await t
        # 仅记录发现开放端口或 ping 成功的主机（减少输出噪声）
        if (r["open_ports"] or r["ping"]):
            results.append(r)
            http_snip = ""
            if r.get("http"):
                http_snip = f"{r['http']['url']} ({r['http']['status']})"
            print(f"[FOUND] {r['ip']} ports={r['open_ports']} ping={r['ping']} {http_snip}")
    dur = time.time() - start
    return results, dur

def shrink_to_local24(net: ipaddress.IPv4Network, ip_str: str) -> ipaddress.IPv4Network:
    """当 net 非常大时，返回以本机 IP 为基准的 /24 子网以避免大规模扫描"""
    ip = ipaddress.IPv4Address(ip_str)
    a,b,c,d = str(ip).split(".")
    return ipaddress.IPv4Network(f"{a}.{b}.{c}.0/24")


def fetch_statuses(discovered: List[Dict], default_ports: List[int], http_timeout: float=3.5, concurrency: int=100):
    # 运行异步任务
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    res = loop.run_until_complete(fetch_statuses_async(discovered, default_ports, http_timeout, concurrency))
    loop.close()
    # 组织输出：把 battery info 合并回 discovered
    merged = []
    for original, r in zip(discovered, res):
        entry = dict(original)  # shallow copy
        entry["status_probe"] = r
        entry["battery"] = None
        if r and r.get("json"):
            b = extract_battery_from_json(r["json"])
            entry["battery"] = b
        elif r and r.get("text"):
            # 尝试从文本里用正则找 voltage number
            import re
            m = re.search(r"([0-9]+(\.[0-9]+)?)\s*(V|v|volt|battery)", r["text"])
            if m:
                try:
                    entry["battery"] = float(m.group(1))
                except:
                    entry["battery"] = None
        merged.append(entry)
    return merged

async def fetch_statuses_async(discovered: List[Dict], default_ports: List[int], http_timeout: float, concurrency: int):
    sem = asyncio.Semaphore(concurrency)
    results = []

    async def worker(ip: str, candidate_ports: List[int]):
        async with sem:
            timeout = aiohttp.ClientTimeout(total=http_timeout)
            async with aiohttp.ClientSession(timeout=timeout) as session:
                # 优先对 open_ports 尝试其端口（如果存在）
                for p in candidate_ports:
                    r = await try_fetch_status(session, ip, p, http_timeout)
                    if r and r.get("status") == 200 and r.get("json") is not None:
                        return r
                    # 也接受 200 但非 json（但优先 json）
                    if r and r.get("status") == 200:
                        return r
                return None

    tasks = []
    for r in discovered:
        ip = r.get("ip")
        # 构建端口优先级：已知 open_ports -> default_ports
        candidate_ports = []
        openp = r.get("open_ports") or []
        for p in openp:
            if p not in candidate_ports:
                candidate_ports.append(p)
        for p in default_ports:
            if p not in candidate_ports:
                candidate_ports.append(p)
        tasks.append(asyncio.create_task(worker(ip, candidate_ports)))

    for t in asyncio.as_completed(tasks):
        try:
            res = await t
            results.append(res)
        except Exception:
            results.append(None)
    return results

# 尝试从 JSON 提取电池字段（宽松匹配常见字段名）
def extract_battery_from_json(j: Dict[str, Any]) -> float:
    if not isinstance(j, dict):
        return None
    # 常见字段名
    for key in ("battery", "batteryLevel", "voltage", "vbat", "battery_level"):
        if key in j:
            try:
                return float(j[key])
            except Exception:
                return None
    # 有时候值放在 nested dict
    # 扫描一级 nested dicts
    for v in j.values():
        if isinstance(v, dict):
            for key in ("battery", "voltage", "batteryLevel", "vbat"):
                if key in v:
                    try:
                        return float(v[key])
                    except Exception:
                        return None
    return None

async def try_fetch_status(session: aiohttp.ClientSession, ip: str, port: int, http_timeout: float):
    scheme = "https" if port == 443 else "http"
    url = f"{scheme}://{ip}:{port}/status"
    try:
        async with session.get(url, timeout=http_timeout, ssl=False) as resp:
            text = await resp.text()
            # 尝试解析 JSON
            try:
                j = await resp.json()
            except Exception:
                # 若返回不是 JSON，尝试从文本中提取数字
                j = None
            return {"ip": ip, "port": port, "url": url, "status": resp.status, "json": j, "text": text[:2000]}
    except Exception as e:
        return None
    
    
def monitor(stdscr, overall_results):

    stdscr.erase()
    stdscr.addstr(0, 0, "IP Monitor (按 q 退出)\n", curses.A_BOLD)

    # 表头
    headers = ["IP", "OpenPorts", "ProbeHTTPStatus", "Battery(V)"]
    # stdscr.addstr(2, 0, "{:<15} {:<20} {:<15} {:<10}".format(*headers), curses.A_UNDERLINE)

    row_y = 4
    for e in overall_results:
        ip = e.get("ip", "")
        open_ports = ",".join(map(str, e.get("open_ports") or []))
        status = (e.get("http") or {}).get("status", "")
        battery = e.get("battery") if e.get("battery") is not None else ""
        
        status_str = f"HTTPStatus({status})" if status else ""
        battery_str = f"battery({round(battery*100,3)}%)" if battery else ""

        line = f"{ip}/{open_ports}\t{status_str}\t{battery_str}                          "

        stdscr.addstr(row_y, 0, line)
        stdscr.clrtoeol()  # 清空光标到行尾的残留字符
        row_y += 1
    stdscr.refresh()
    
        
def main(stdscr):
    parser = argparse.ArgumentParser(description="自动根据本机所有 IPv4 地址扫描对应子网 (TCP + HTTP probe)")
    parser.add_argument("--ports", "-p", default=",".join(map(str, DEFAULT_PORTS)), help="逗号分隔端口列表")
    parser.add_argument("--concurrency", "-c", type=int, default=CONCURRENCY_DEFAULT)
    parser.add_argument("--tcp-timeout", type=float, default=TCP_TIMEOUT_DEFAULT)
    parser.add_argument("--http-timeout", type=float, default=HTTP_TIMEOUT_DEFAULT)
    parser.add_argument("--no-ping", action="store_true", help="禁用 ICMP ping")
    parser.add_argument("--allow-large-nets", action="store_true", help="允许扫描大网段（可能非常慢）")
    parser.add_argument("--ifaces", type=str, default="", help="只扫描这些接口（逗号分隔），默认所有活动接口")
    args = parser.parse_args()

    iface_filter = [s.strip() for s in args.ifaces.split(",")] if args.ifaces else None
    nets = enumerate_ipv4_networks(iface_filter)
    if not nets:
        print("未检测到任何活动 IPv4 接口地址。")
        return

    ports = [int(x) for x in args.ports.split(",") if x.strip()]
    do_ping = not args.no_ping

    print(f"[+] 检测到接口/网段: ")
    for iface, net, ip in nets:
        print(f"    {iface:20s}  {ip:15s}  ->  {net}")

    overall_results = []
    total_time = 0.0

    for iface, net, ip in nets:
        scan_net = net
        large = net.num_addresses > LARGE_NET_THRESHOLD
        if (large and not args.allow_large_nets) and ip != '192.168.137.1':
            # print(f"[!] 网段 {net} 较大（{net.num_addresses} 地址），默认缩小为本机同 /24 ({ip})。可用 --allow-large-nets 强制完整扫描。")
            # scan_net = shrink_to_local24(net, ip)
            print(ip, net.num_addresses)
        else:    
            print(f"[+] 开始扫描接口 {iface} 的网段 {scan_net} (hosts={scan_net.num_addresses - 2}) ...")
            results, dur = asyncio.run(scan_network_hosts(scan_net, ports, do_ping, args.tcp_timeout, args.http_timeout, args.concurrency))
            total_time += dur
            overall_results.extend(results)
            print(f"[+] 完成 {scan_net}，耗时 {dur:.1f}s，发现 {len(results)} 个响应主机\n")

    # 汇总输出
    rows = []
    for r in overall_results:
        ip = r["ip"]
        ping = str(r["ping"])
        ports_s = ",".join(map(str, r.get("open_ports", [])))
        http_s = ""
        if r.get("http"):
            http_s = f"{r['http'].get('status')} {r['http'].get('url')}"
        rows.append([ip, ping, ports_s, http_s])
    headers = ["IP", "Ping", "OpenPorts", "HTTP status/url"]
    if rows:
        if tabulate:
            print(tabulate(rows, headers=headers, tablefmt="github"))
        else:
            print(headers)
            for row in rows:
                print(row)
    else:
        print("[+] 未在任何子网内发现开放端口或 ping 回复的主机（可能设备未暴露常见端口或被防火墙阻挡）")

    print(f"[+] 总耗时: {total_time:.1f}s")

    # print(overall_results)
    # 每 60 秒循环更新
    curses.curs_set(0)         # 隐藏光标
    stdscr.nodelay(True)       # 非阻塞输入
    stdscr.clear()

    while True:
        
        merged = []

        for r in overall_results:
            http_info = r.get("http")
            if http_info and http_info.get("status") == 200:
                entry = dict(r)
                entry["battery"] = None  # 默认没有电压
            
                url = http_info.get("url")
                # 确保访问 /status
                status_url = url.rstrip("/") + "/status"
                try:
                    resp = requests.get(status_url, timeout=3)
                    if resp.status_code == 200:
                        j = resp.json()
                        # 尝试提取电压
                        for key in ("battery", "batteryLevel", "voltage", "vbat", "battery_level"):
                            if key in j:
                                entry["battery"] = ( float(j[key]) - 3.6)/(4.2-3.6)
                                break
                except Exception as e:
                    pass  # 跳过访问失败或非 JSON
                merged.append(entry)
                
        monitor(stdscr, merged)
        
        time.sleep(20)
    
if __name__ == "__main__":
    curses.wrapper(main)
    
