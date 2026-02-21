import streamlit as st
import pandas as pd
import json
import os
import time
import subprocess

st.set_page_config(page_title="C++ Matching Engine Profiler", layout="wide")

# ==========================================
# SIDEBAR: The Simulation Controller
# ==========================================
# ==========================================
# SIDEBAR: The Simulation Controller
# ==========================================
st.sidebar.header("🕹️ Simulation Control")

# 1. The Mode Toggle
mode = st.sidebar.radio("C++ Engine Architecture", ["Lock-Free Queue", "Synchronous Mutex"])
with open("engine_mode.txt", "w") as f:
    f.write("queue" if mode == "Lock-Free Queue" else "sync")

# 2. The Load Parameters
num_threads = st.sidebar.number_input("Concurrent Clients (Threads)", min_value=1, max_value=500, value=50)
orders_per = st.sidebar.number_input("Orders per Client", min_value=100, max_value=100000, value=4000)

if st.sidebar.button("🚀 LAUNCH LOAD TEST", use_container_width=True):
    try:
        # Pass the UI values into the Python script as command line arguments
        subprocess.Popen(["python3", "load_generator.py", str(num_threads), str(orders_per)])
        st.sidebar.success(f"Fired {num_threads * orders_per:,} orders into the engine!")
    except Exception as e:
        st.sidebar.error(f"Failed to start: {e}")

st.sidebar.markdown("---")
refresh_rate = st.sidebar.slider("UI Refresh Rate (sec)", 0.1, 2.0, 0.5)
freeze = st.sidebar.checkbox("Freeze UI")

# ==========================================
# MAIN UI: Metrics & Throughput
# ==========================================
st.title("⚡ C++ Lock-Free Matching Engine")
st.markdown("Live Performance Profiler & Depth of Market")

if "ops_history" not in st.session_state:
    st.session_state.ops_history = [0] * 60 # Pre-fill with 60 seconds of zeros

def safe_read_json(filepath):
    if os.path.exists(filepath):
        try:
            with open(filepath, "r") as f:
                return json.load(f)
        except:
            return None
    return None

if not freeze:
    # 1. Process Metrics (Throughput)
    metrics = safe_read_json("metrics.json")
    if metrics:
        network_ops = metrics.get("network_ops", 0)
        engine_ops = metrics.get("engine_ops", 0)
        total_engine = metrics.get("total_engine", 0)
        
        active_ops = engine_ops if engine_ops > 0 else network_ops

        # Update chart memory
        st.session_state.ops_history.append(active_ops)
        if len(st.session_state.ops_history) > 60:
            st.session_state.ops_history.pop(0)

        # Big KPIs
        c1, c2, c3 = st.columns(3)
        c1.metric(label="Live Throughput", value=f"{active_ops:,.0f} OPS")
        c2.metric(label="Total Orders Processed", value=f"{total_engine:,.0f}")
        c3.metric(label="Engine Status", value="🔥 PROCESSING" if active_ops > 0 else "🟢 IDLE")

        # OPS Line Chart
        st.subheader("System Throughput (Orders Per Second)")
        chart_data = pd.DataFrame({"OPS": st.session_state.ops_history})
        st.line_chart(chart_data, height=250, color="#FF4B4B")

    # 2. Process Order Book (Depth of Market)
    st.markdown("---")
    st.subheader("📊 Depth of Market (DOM)")
    
    book_data = safe_read_json("book_state.json")
    if book_data:
        bids = pd.DataFrame(book_data.get("bids", []))
        asks = pd.DataFrame(book_data.get("asks", []))

        # We need to merge them to draw a cool Histogram where X is Price and Y is Quantity
        dom_data = []
        
        if not bids.empty:
            bids["Side"] = "Bid"
            dom_data.append(bids)
        if not asks.empty:
            asks["Side"] = "Ask"
            dom_data.append(asks)
            
        if dom_data:
            df = pd.concat(dom_data)
            df["price"] = df["price"] / 100.0 # Format price correctly
            
            # Pivot table so Streamlit can draw overlapping colored bars
            chart_df = df.pivot(index="price", columns="Side", values="quantity").fillna(0)
            
            # Map colors: Bids are Green, Asks are Red
            color_map = []
            if "Bid" in chart_df.columns: color_map.append("#00FF00")
            if "Ask" in chart_df.columns: color_map.append("#FF0000")
            
            st.bar_chart(chart_df, height=300, color=color_map if color_map else None)
        else:
            st.info("Order book is currently empty.")

    time.sleep(refresh_rate)
    st.rerun()
else:
    st.warning("UI is Frozen for Inspection.")