'use client';
import { useState, useRef, useEffect } from 'react';

// Math Constants based on prompt requirements
const UNITS_PER_METER = 5;
const VISUAL_SCALE = 35; // 1 unit = 35 pixels for the UI display

// Coordinate mapping (X, Y, Z in mathematical units)
const NODES = {
  head: { x: 0, y: 0, z: 3.5, label: 'Head (LiDAR)' },
  arm1: { x: 1.65, y: 0, z: 0, label: 'Right Arm' },
  arm2: { x: 0, y: 1.65, z: 0, label: 'Left Arm' }
};

export default function Dashboard() {
  // Obstacle state (represented as a 3D sphere)
  const [obsX, setObsX] = useState(0);
  const [obsY, setObsY] = useState(0);
  const [obsZ, setObsZ] = useState(1.75); // Slider controls height (default waist height)
  
  // Haptic state (number of active motors per section: 0 to 3)
  const [haptics, setHaptics] = useState({ head: 0, arm1: 0, arm2: 0 });
  
  const radarRef = useRef<HTMLDivElement>(null);

  // Calculates 3D distance and assigns motor thresholds
  const calculateHaptics = (x: number, y: number, z: number) => {
    const newHaptics = { head: 0, arm1: 0, arm2: 0 };
    
    (Object.keys(NODES) as Array<keyof typeof NODES>).forEach((key) => {
      const node = NODES[key];
      // 3D Distance Formula: d = sqrt((x2-x1)^2 + (y2-y1)^2 + (z2-z1)^2)
      const distUnits = Math.sqrt(
        Math.pow(x - node.x, 2) + Math.pow(y - node.y, 2) + Math.pow(z - node.z, 2)
      );
      
      const distMeters = distUnits / UNITS_PER_METER;

      // Haptic Activation Logic
      if (distMeters <= 0.5) newHaptics[key] = 3;
      else if (distMeters <= 1.0) newHaptics[key] = 2;
      else if (distMeters <= 1.5) newHaptics[key] = 1;
      else newHaptics[key] = 0;
    });

    setHaptics(newHaptics);
  };

  const handleMouseMove = (e: React.MouseEvent) => {
    if (!radarRef.current) return;
    const rect = radarRef.current.getBoundingClientRect();
    
    // Map mouse position to coordinates (Center is 0,0)
    // Invert Y so standard cartesian (+Y is up) applies
    const mousePixelX = e.clientX - rect.left - rect.width / 2;
    const mousePixelY = -(e.clientY - rect.top - rect.height / 2);

    const mathX = mousePixelX / VISUAL_SCALE;
    const mathY = mousePixelY / VISUAL_SCALE;

    setObsX(mathX);
    setObsY(mathY);
    calculateHaptics(mathX, mathY, obsZ);
  };

  // Recalculate if Z slider changes
  useEffect(() => {
    calculateHaptics(obsX, obsY, obsZ);
  }, [obsZ]);

  // UI Component for rendering 3 motors per body section
  const MotorGroup = ({ count, label }: { count: number, label: string }) => {
    const isCritical = count === 3;
    return (
      <div className="bg-gray-800/50 p-4 rounded-xl border border-gray-700">
        <div className="flex justify-between items-center mb-3">
          <h3 className="text-sm font-bold text-gray-300">{label}</h3>
          <span className={`text-xs font-mono px-2 py-1 rounded ${count > 0 ? 'bg-amber-500/20 text-amber-400' : 'bg-gray-700 text-gray-400'}`}>
            {count}/3 Active
          </span>
        </div>
        <div className="grid grid-cols-3 gap-2">
          {[1, 2, 3].map((motorIndex) => (
             <div 
             key={motorIndex} 
             className={`h-8 rounded-md transition-all duration-75 flex items-center justify-center font-mono text-xs font-bold
               ${motorIndex <= count 
                 ? isCritical 
                    ? 'bg-red-500 text-white shadow-[0_0_15px_rgba(239,68,68,0.6)] animate-[pulse_0.1s_ease-in-out_infinite]' // Very fast vibration
                    : 'bg-amber-400 text-gray-900 shadow-[0_0_10px_rgba(251,191,36,0.4)]'
                 : 'bg-gray-900 border border-gray-700 text-gray-600'
               }`}
           >
             M{motorIndex}
           </div>
          ))}
        </div>
        {isCritical && <p className="text-[10px] text-red-400 mt-2 font-bold uppercase tracking-wider text-center animate-pulse">Critical Proximity</p>}
      </div>
    );
  };

  return (
    <div className="min-h-screen bg-gray-950 text-gray-100 p-8 font-sans">
      <div className="max-w-6xl mx-auto">
        <header className="mb-8 border-b border-gray-800 pb-6 flex justify-between items-end">
          <div>
            <h1 className="text-3xl font-extrabold text-white flex items-center gap-3">
              <span className="w-3 h-3 rounded-full bg-blue-500 animate-pulse"></span>
              Wheelchair Spatial LiDAR Demo
            </h1>
            <p className="text-sm text-gray-400 mt-2">
              Math Engine: 5 units = 1m | Obstacle Trigger: 1.5m (1x), 1.0m (2x), 0.5m (3x)
            </p>
          </div>
          <div className="text-right">
            <p className="text-xs text-gray-500">Obstacle Coords (Units)</p>
            <p className="text-lg font-mono text-blue-400">
              X: {obsX.toFixed(2)} | Y: {obsY.toFixed(2)} | Z: {obsZ.toFixed(2)}
            </p>
          </div>
        </header>

        <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
          
          {/* Left Panel: Haptic Feedback Matrix */}
          <div className="lg:col-span-4 flex flex-col gap-4">
            <div className="bg-gray-900 rounded-2xl p-6 border border-gray-800 shadow-xl">
              <h2 className="text-sm font-bold uppercase tracking-widest text-gray-500 mb-6 border-b border-gray-800 pb-2">
                9-Zone Haptic Array
              </h2>
              <div className="flex flex-col gap-4">
                <MotorGroup count={haptics.head} label={NODES.head.label} />
                <MotorGroup count={haptics.arm2} label={NODES.arm2.label} />
                <MotorGroup count={haptics.arm1} label={NODES.arm1.label} />
              </div>
            </div>

            {/* Z-Axis Height Slider */}
            <div className="bg-gray-900 rounded-2xl p-6 border border-gray-800 shadow-xl">
              <h2 className="text-sm font-bold uppercase tracking-widest text-gray-500 mb-4">
                Obstacle Z-Height (Elevation)
              </h2>
              <input 
                type="range" 
                min="-2" 
                max="6" 
                step="0.1" 
                value={obsZ}
                onChange={(e) => setObsZ(parseFloat(e.target.value))}
                className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer accent-blue-500"
              />
              <div className="flex justify-between text-xs text-gray-400 mt-2 font-mono">
                <span>Floor (Z: -2)</span>
                <span>Head Level (Z: 3.5)</span>
                <span>Above (Z: 6)</span>
              </div>
            </div>
          </div>

          {/* Right Panel: Interactive Top-Down Radar */}
          <div className="lg:col-span-8 bg-gray-900 rounded-2xl p-6 border border-gray-800 shadow-xl flex flex-col">
            <h2 className="text-sm font-bold uppercase tracking-widest text-gray-500 mb-4 flex justify-between">
              <span>Top-Down Radar Zone</span>
              <span className="text-blue-400 lowercase">Hover to move obstacle sphere</span>
            </h2>
            
            <div 
              ref={radarRef}
              onMouseMove={handleMouseMove}
              className="flex-grow w-full rounded-xl bg-gray-950 border border-gray-800 relative overflow-hidden cursor-crosshair min-h-[500px]"
            >
              {/* Grid Lines */}
              <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.03)_1px,transparent_1px)] bg-[size:35px_35px]" />
              
              {/* X and Y Axes */}
              <div className="absolute top-1/2 left-0 w-full h-px bg-gray-700/50" />
              <div className="absolute top-0 left-1/2 w-px h-full bg-gray-700/50" />

              {/* Render Nodes (Head, Arms) and their Proximity Rings */}
              {Object.entries(NODES).map(([key, node]) => {
                // Convert Math Coordinates to Pixel Offsets from Center
                const pixelX = `calc(50% + ${node.x * VISUAL_SCALE}px)`;
                const pixelY = `calc(50% - ${node.y * VISUAL_SCALE}px)`;

                return (
                  <div key={key} className="absolute" style={{ left: pixelX, top: pixelY, transform: 'translate(-50%, -50%)' }}>
                    {/* 1.5m Ring (7.5 units) */}
                    <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 rounded-full border border-gray-700/30" 
                         style={{ width: `${1.5 * UNITS_PER_METER * VISUAL_SCALE * 2}px`, height: `${1.5 * UNITS_PER_METER * VISUAL_SCALE * 2}px` }} />
                    
                    {/* Node Dot */}
                    <div className={`w-4 h-4 rounded-full border-2 border-gray-950 z-10 relative
                      ${key === 'head' ? 'bg-purple-500 shadow-[0_0_10px_rgba(168,85,247,0.8)]' : 'bg-emerald-500 shadow-[0_0_10px_rgba(16,185,129,0.8)]'}
                    `} />
                    <span className="absolute top-4 left-1/2 -translate-x-1/2 text-[10px] text-gray-400 font-mono whitespace-nowrap mt-1 bg-gray-950/80 px-1 rounded">
                      {node.label}
                    </span>
                  </div>
                );
              })}

              {/* Render the User-Controlled Obstacle Sphere */}
              <div 
                className="absolute w-8 h-8 rounded-full border-2 border-white/20 shadow-[0_0_20px_rgba(59,130,246,0.6)] backdrop-blur-sm bg-blue-500/20 z-20 transition-transform duration-75 pointer-events-none flex items-center justify-center"
                style={{
                  left: `calc(50% + ${obsX * VISUAL_SCALE}px)`,
                  top: `calc(50% - ${obsY * VISUAL_SCALE}px)`,
                  transform: 'translate(-50%, -50%)',
                  // Scale visually slightly based on Z height for a faux-3D effect
                  transform: `translate(-50%, -50%) scale(${Math.max(0.5, 1 + (obsZ / 10))})`
                }}
              >
                <div className="w-2 h-2 bg-blue-400 rounded-full" />
              </div>
            </div>
            <div className="mt-4 flex justify-center gap-6 text-xs text-gray-500 font-mono">
              <span className="flex items-center gap-2"><div className="w-3 h-3 bg-purple-500 rounded-full"></div> LiDAR Sensor (Z: 3.5)</span>
              <span className="flex items-center gap-2"><div className="w-3 h-3 bg-emerald-500 rounded-full"></div> Arm Sensors (Z: 0.0)</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}