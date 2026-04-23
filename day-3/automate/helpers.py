from pathlib import Path
import xml.etree.ElementTree as ET

from pathlib import Path
import pandas as pd

def _get_text(root, xpath, default=None):
    node = root.find(xpath)
    return node.text.strip() if (node is not None and node.text is not None) else default

def parse_csynth_xml(csynth_path):
    csynth_path = Path(csynth_path)
    if not csynth_path.exists():
        raise FileNotFoundError(f"csynth.xml not found: {csynth_path}")

    root = ET.parse(csynth_path).getroot()

    # -------- Timing / latency --------
    target_clk_ns = _get_text(root, "./UserAssignments/TargetClockPeriod")
    est_clk_ns    = _get_text(root, "./PerformanceEstimates/SummaryOfTimingAnalysis/EstimatedClockPeriod")

    lat_best_cyc  = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Best-caseLatency")
    lat_avg_cyc   = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Average-caseLatency")
    lat_worst_cyc = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Worst-caseLatency")

    rt_best       = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Best-caseRealTimeLatency")
    rt_avg        = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Average-caseRealTimeLatency")
    rt_worst      = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Worst-caseRealTimeLatency")

    ii_min        = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Interval-min")
    ii_max        = _get_text(root, "./PerformanceEstimates/SummaryOfOverallLatency/Interval-max")

    # -------- Resources --------
    res_fields = ["LUT", "FF", "DSP", "BRAM_18K", "URAM"]
    used = {k: _get_text(root, f"./AreaEstimates/Resources/{k}", "0") for k in res_fields}
    avail = {k: _get_text(root, f"./AreaEstimates/AvailableResources/{k}", "0") for k in res_fields}

    # Convert to ints 
    used_i  = {k: int(str(v).replace(",", "")) for k, v in used.items()}
    avail_i = {k: int(str(v).replace(",", "")) for k, v in avail.items()}

    # Percent utilization
    util_pct = {}
    for k in res_fields:
        a = avail_i.get(k, 0)
        util_pct[k] = (100.0 * used_i[k] / a) if a else None

    # Extra metadata (optional)
    part = _get_text(root, "./UserAssignments/Part")
    top  = _get_text(root, "./UserAssignments/TopModelName")

    return {
        "meta": {
            "part": part,
            "top": top,
            "target_clock_ns": float(target_clk_ns) if target_clk_ns else None,
            "estimated_clock_ns": float(est_clk_ns) if est_clk_ns else None,
        },
        "latency": {
            "best_cycles": int(lat_best_cyc) if lat_best_cyc else None,
            "avg_cycles": int(lat_avg_cyc) if lat_avg_cyc else None,
            "worst_cycles": int(lat_worst_cyc) if lat_worst_cyc else None,
            "best_time": rt_best,
            "avg_time": rt_avg,
            "worst_time": rt_worst,
            "ii_min": int(ii_min) if ii_min else None,
            "ii_max": int(ii_max) if ii_max else None,
        },
        "resources": {
            "used": used_i,
            "available": avail_i,
            "util_percent": util_pct,
        }
    }

def csynth_xml_path(PATH_HLS_PROJECT):
    return (Path(PATH_HLS_PROJECT) /
            "solution1" / "syn" / "report" / "csynth.xml")


def hls_report_to_dataframe(report):
    res = report["resources"]

    df = pd.DataFrame({
        "Used": res["used"],
        "Available": res["available"],
        "Util (%)": {k: round(v,2) if v else None for k,v in res["util_percent"].items()}
    })

    return df


def csynth_xml_from_project_root(project_root, solution="solution1"):
    project_root = Path(project_root)
    return project_root / solution / "syn" / "report" / "csynth.xml"

def impl_ip_dir_from_project_root(project_root, solution="solution1"):
    project_root = Path(project_root)
    return project_root / solution / "impl" / "ip"

def list_impl_ip(project_root, solution="solution1"):
    ip_dir = impl_ip_dir_from_project_root(project_root, solution)
    zips = sorted(ip_dir.glob("*.zip"))
    dirs = sorted([p for p in ip_dir.iterdir() if p.is_dir()])
    return ip_dir, zips, dirs



import zipfile
import shutil

def _is_ip_root(p: Path) -> bool:
    """Heurística para detectar raíz de un IP extraído."""
    if not p.is_dir():
        return False
    if (p / "component.xml").exists():
        return True
    # otras estructuras comunes
    if (p / "xgui").is_dir():
        return True
    if (p / "hdl").is_dir():
        return True
    return False

def export_hls_ip(
    project_root,
    dest_root="ip",
    project_name="hls4ml_prj_qap_qkeras_accel_prj",
    solution="solution1",
    clean_dest=False,
):
    project_root = Path(project_root)
    dest_root = Path(dest_root) / project_name
    dest_root.mkdir(parents=True, exist_ok=True)

    if clean_dest:
        # borra contenido previo sin borrar la carpeta
        for item in dest_root.iterdir():
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

    # 1) Buscar ZIP del IP
    ip_dir = project_root / solution / "impl" / "ip"
    zips = sorted(ip_dir.glob("*.zip"))
    if not zips:
        raise FileNotFoundError(f"No IP zip found in {ip_dir}")

    ip_zip = zips[0]

    # 2) Copiar ZIP
    dst_zip = dest_root / ip_zip.name
    shutil.copy2(ip_zip, dst_zip)

    # 3) Extraer
    with zipfile.ZipFile(dst_zip, "r") as zf:
        zf.extractall(dest_root)

    # 4) Detectar carpeta raíz del IP:
    #    (a) si el ZIP creó una carpeta “xilinx_com_hls_*” al nivel superior
    direct = [p for p in dest_root.iterdir() if p.is_dir() and "xilinx_com_hls" in p.name]
    for p in direct:
        if _is_ip_root(p) or any(_is_ip_root(x) for x in p.rglob("*")):
            return {"zip": dst_zip, "ip_dir": p}

    #    (b) si el IP está en una carpeta con otro nombre, buscar por contenido recursivo
    candidates = [p for p in dest_root.rglob("*") if _is_ip_root(p)]
    if candidates:
        # devolver el “más cercano” a dest_root (menos profundo)
        candidates.sort(key=lambda x: len(x.relative_to(dest_root).parts))
        return {"zip": dst_zip, "ip_dir": candidates[0]}

    #    (c) si NO hay carpeta raíz, capaz se extrajo “plano” en dest_root
    if _is_ip_root(dest_root):
        return {"zip": dst_zip, "ip_dir": dest_root}

    # Debug: listar top-level para ver qué creó el zip
    top = sorted([p.name + ("/" if p.is_dir() else "") for p in dest_root.iterdir()])
    raise RuntimeError(
        "ZIP extracted but IP root not found. Top-level contents:\n- " + "\n- ".join(top[:50])
    )
