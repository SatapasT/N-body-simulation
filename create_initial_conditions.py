import argparse
import math
import random

def create_particles(N, min_mass, max_mass, scenario):
    particles = []
    if scenario == "no-collisions":
        cube_root = math.ceil(N ** (1/3))
        if cube_root**3 != N:
            print(f"Warning: cube_root^3 = {cube_root**3}, differs from input N = {N}")
            N = cube_root**3
        spacing = 2.0
        origin = - (cube_root - 1) * spacing / 2

    for idx in range(N):
        if scenario == "random-grid":
            xPos = random.random()
            yPos = random.random()
            zPos = random.random()
            vx = vy = vz = 0.0

        elif scenario == "no-collisions":
            x_i = idx % cube_root
            y_i = (idx // cube_root) % cube_root
            z_i = idx // (cube_root * cube_root)
            xPos = origin + x_i * spacing
            yPos = origin + y_i * spacing
            zPos = origin + z_i * spacing

            dx = xPos
            dy = yPos
            dz = zPos
            norm = math.sqrt(dx*dx + dy*dy + dz*dz) + 1e-12
            speed = 0.1
            vx = speed * dx / norm
            vy = speed * dy / norm
            vz = speed * dz / norm

        elif scenario == "shock":
            xPos = random.random()
            yPos = random.random()
            zPos = random.random()
            dist = math.sqrt((xPos - 0.5)**2 + (yPos - 0.5)**2 + (zPos - 0.5)**2)
            if dist < 0.1:
                factor = 1.0 / (dist + 1e-5)
                vx = (xPos - 0.5) * factor
                vy = (yPos - 0.5) * factor
                vz = (zPos - 0.5) * factor
            else:
                vx = vy = vz = 0.0

        else:
            raise ValueError(f"Unsupported scenario: {scenario}")

        mass = random.uniform(min_mass, max_mass)
        particles.append((xPos, yPos, zPos, vx, vy, vz, mass))

    return particles


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate particle initial conditions for an N-body simulation.")
    parser.add_argument("--dt", type=float, required=True, help="Time step size (e.g., 0.01)")
    parser.add_argument("--final-time", type=float, required=True, help="Total simulation time (e.g., 100.0)")
    parser.add_argument("--snapshots", type=float, required=True, help="Snapshot interval for output (e.g., 0.001)")
    parser.add_argument("--N", type=int, required=True, help="Number of particles to generate, must be cubic number for no-collisions scenario")
    parser.add_argument("--min-mass", type=float, required=True, help="Minimum particle mass")
    parser.add_argument("--max-mass", type=float, required=True, help="Maximum particle mass")
    parser.add_argument("--scenario",
    choices=["random-grid", "no-collisions", "shock"],
    default="random-grid",
    help="Initial condition scenario: 'random-grid' (random positions, zero velocity), "
        "'no-collisions' (spaced particles with outward motion, use a small final time such as 1.0 for this scenario otherwise collisions may still occur), "
        "'shock' (dense central cluster with high velocities)"
    )
    parser.add_argument("--output", type=str, required=True, help="Output filename for initial conditions")

    args = parser.parse_args()

    if args.scenario == "no-collisions" and args.final_time >1.0:
        print("The chosen time must remain small (<1.0) to ensure no collisions occur")
    particles = create_particles(args.N, args.min_mass, args.max_mass, args.scenario)

    with open(args.output, "w") as f:
        f.write(f"{args.dt} {args.final_time} {args.snapshots}\n")
        for p in particles:
            f.write(" ".join(f"{v:.6f}" for v in p) + "\n")

    print(f"Generated {len(particles)} particles in '{args.output}'")

