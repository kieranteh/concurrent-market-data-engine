import React, { useState, useEffect } from 'react';
import io from 'socket.io-client';

// Connect to the backend server
const socket = io('http://localhost:4000');

function App() {
  const [stats, setStats] = useState({ processed: 0, trades: 0, throughput: 0 });
  const [biggestTrades, setBiggestTrades] = useState([]);

  useEffect(() => {
    // Listen for 'stats' events from the backend
    socket.on('stats', (data) => {
      // Check if the data is a trade execution (has symbol/price) or general stats
      if (data.symbol && data.price && data.quantity) {
        setBiggestTrades(prev => {
          const newTrades = [...prev, data];
          // Sort by total value (price * quantity) descending
          newTrades.sort((a, b) => (b.price * b.quantity) - (a.price * a.quantity));
          // Keep only top 10
          return newTrades.slice(0, 10);
        });
      } else if (data.processed !== undefined) {
        setStats(data);
      }
    });

    // Cleanup on unmount
    return () => socket.off('stats');
  }, []);

  const handleRestart = () => {
    socket.emit('restart_engine');
    // Reset UI state
    setStats({ processed: 0, trades: 0, throughput: 0 });
    setBiggestTrades([]);
  };

  return (
    <div style={{ padding: '50px', fontFamily: 'Arial, sans-serif', backgroundColor: '#282c34', minHeight: '100vh', color: 'white' }}>
      <h1 style={{ textAlign: 'center', marginBottom: '40px' }}>🚀 Concurrent Market Data Engine</h1>
      
      <div style={{ display: 'flex', justifyContent: 'center', gap: '30px', flexWrap: 'wrap' }}>
        <StatCard title="Total Events Processed" value={stats.processed.toLocaleString()} color="#61dafb" />
        <StatCard title="Trades Executed" value={stats.trades.toLocaleString()} color="#91fb61" />
        <StatCard title="Throughput (events/sec)" value={stats.throughput.toLocaleString()} color="#fb61da" />
      </div>

      <div style={{ textAlign: 'center', marginTop: '40px' }}>
        <button 
          onClick={handleRestart}
          style={{
            padding: '15px 30px',
            fontSize: '18px',
            backgroundColor: '#e94560',
            color: 'white',
            border: 'none',
            borderRadius: '8px',
            cursor: 'pointer',
            fontWeight: 'bold'
          }}
        >
          🔄 Restart Engine
        </button>
      </div>

      <div style={{ marginTop: '50px', maxWidth: '800px', margin: '50px auto' }}>
        <h2 style={{ borderBottom: '1px solid #555', paddingBottom: '10px' }}>🏆 Biggest Trades Log</h2>
        <table style={{ width: '100%', borderCollapse: 'collapse', marginTop: '20px' }}>
          <thead>
            <tr style={{ textAlign: 'left', color: '#888' }}>
              <th style={{ padding: '10px' }}>Symbol</th>
              <th style={{ padding: '10px' }}>Side</th>
              <th style={{ padding: '10px' }}>Price</th>
              <th style={{ padding: '10px' }}>Quantity</th>
              <th style={{ padding: '10px' }}>Total Value</th>
            </tr>
          </thead>
          <tbody>
            {biggestTrades.map((trade, index) => (
              <tr key={index} style={{ borderBottom: '1px solid #333' }}>
                <td style={{ padding: '10px', fontWeight: 'bold', color: '#61dafb' }}>{trade.symbol}</td>
                <td style={{ padding: '10px', color: trade.side === 'B' ? '#91fb61' : '#ff6b6b' }}>{trade.side === 'B' ? 'BUY' : 'SELL'}</td>
                <td style={{ padding: '10px' }}>${trade.price.toFixed(2)}</td>
                <td style={{ padding: '10px' }}>{trade.quantity}</td>
                <td style={{ padding: '10px', fontWeight: 'bold' }}>${(trade.price * trade.quantity).toLocaleString()}</td>
              </tr>
            ))}
            {biggestTrades.length === 0 && (
              <tr>
                <td colSpan="5" style={{ padding: '20px', textAlign: 'center', color: '#666' }}>No trades executed yet...</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function StatCard({ title, value, color }) {
  return (
    <div style={{ 
      border: `2px solid ${color}`, 
      padding: '20px', 
      borderRadius: '15px', 
      width: '250px',
      textAlign: 'center',
      backgroundColor: 'rgba(0,0,0,0.3)'
    }}>
      <h3 style={{ margin: '0 0 10px 0', fontSize: '18px', color: '#ddd' }}>{title}</h3>
      <p style={{ fontSize: '32px', fontWeight: 'bold', margin: 0, color: color }}>{value}</p>
    </div>
  );
}

export default App;
