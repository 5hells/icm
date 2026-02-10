/**
 * Builder for creating pixel manipulation effects using mathematical equations
 */
export class PixelEffectBuilder {
    private definitions: string[] = [];
    private functions: string[] = [];
    private equations: string[] = [];

    /**
     * Define a float variable
     * @param name Variable name
     * @param value Variable value
     */
    defineFloat(name: string, value: number): this {
        this.definitions.push(`deff ${name} ${value}`);
        return this;
    }

    /**
     * Define an integer variable
     * @param name Variable name
     * @param value Variable value
     */
    defineInt(name: string, value: number): this {
        this.definitions.push(`defi ${name} ${value}`);
        return this;
    }

    /**
     * Define a function
     * @param name Function name
     * @param params Parameter list
     * @param body Function body
     */
    defineFunction(name: string, params: string, body: string): this {
        this.functions.push(`defn ${name}(${params}) {\n${body}\n}`);
        return this;
    }

    /**
     * Add a blur effect with configurable radius
     * @param radius Blur radius (default 5.0)
     */
    blur(radius: number = 5.0): this {
        this.defineFloat('blur_radius', radius);
        this.defineFunction('blur_sample', 'dx, dy', `
        int sample_x = clamp(x + dx, 0, width - 1);
        int sample_y = clamp(y + dy, 0, height - 1);
        int idx = (sample_y * width + sample_x) * 4;
        return [pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]];
    `);
        this.defineFunction('blur', '', `
        chunk4 result = [0, 0, 0, 0];
        int count = 0;
        for (int dx = -blur_radius; dx <= blur_radius; dx++) {
            for (int dy = -blur_radius; dy <= blur_radius; dy++) {
                chunk4 sample = blur_sample(dx, dy);
                result[0] += sample[0];
                result[1] += sample[1];
                result[2] += sample[2];
                result[3] += sample[3];
                count++;
            }
        }
        result[0] /= count;
        result[1] /= count;
        result[2] /= count;
        result[3] /= count;
        return result;
    `);
        this.equations.push(`chunk4*:[r, g, b, a] = blur();`);
        return this;
    }

    /**
     * Add a red channel equation
     * @param expr Expression using r, g, b, a variables (0-255 range)
     */
    red(expr: string): this {
        this.equations.push(`r = ${expr};`);
        return this;
    }

    /**
     * Add a green channel equation
     * @param expr Expression using r, g, b, a variables (0-255 range)
     */
    green(expr: string): this {
        this.equations.push(`g = ${expr};`);
        return this;
    }

    /**
     * Add a blue channel equation
     * @param expr Expression using r, g, b, a variables (0-255 range)
     */
    blue(expr: string): this {
        this.equations.push(`b = ${expr};`);
        return this;
    }

    /**
     * Add an alpha channel equation
     * @param expr Expression using r, g, b, a variables (0-255 range)
     */
    alpha(expr: string): this {
        this.equations.push(`a = ${expr};`);
        return this;
    }

    /**
     * Add a custom equation
     * @param equation Full equation string
     */
    custom(equation: string): this {
        this.equations.push(equation);
        return this;
    }

    /**
     * Apply a brightness adjustment
     * @param factor Brightness multiplier (0.0 = black, 1.0 = normal, >1.0 = brighter)
     */
    brightness(factor: number): this {
        this.red(`r * ${factor}`);
        this.green(`g * ${factor}`);
        this.blue(`b * ${factor}`);
        return this;
    }

    /**
     * Apply a contrast adjustment
     * @param factor Contrast factor (0.0 = flat, 1.0 = normal, >1.0 = more contrast)
     */
    contrast(factor: number): this {
        const offset = 128 * (1 - factor);
        this.red(`(r - 128) * ${factor} + 128 + ${offset}`);
        this.green(`(g - 128) * ${factor} + 128 + ${offset}`);
        this.blue(`(b - 128) * ${factor} + 128 + ${offset}`);
        return this;
    }

    /**
     * Apply a sepia tone effect
     */
    sepia(): this {
        this.red(`r * 0.393 + g * 0.769 + b * 0.189`);
        this.green(`r * 0.349 + g * 0.686 + b * 0.168`);
        this.blue(`r * 0.272 + g * 0.534 + b * 0.131`);
        return this;
    }

    /**
     * Apply a grayscale effect
     */
    grayscale(): this {
        this.custom(`let gray = r * 0.299 + g * 0.587 + b * 0.114; r = gray; g = gray; b = gray;`);
        return this;
    }

    /**
     * Apply a threshold effect
     */
    threshold(threshold: number): this {
        this.custom(`let value = (r + g + b) / 3; r = value > ${threshold} ? 255 : 0; g = value > ${threshold} ? 255 : 0; b = value > ${threshold} ? 255 : 0;`);
        return this;
    }

    /**
     * Apply a color overlay effect
     */
    colorOverlay(r: number, g: number, b: number, a: number): this {
        this.red(`(r * (255 - ${a}) + ${r} * ${a}) / 255`);
        this.green(`(g * (255 - ${a}) + ${g} * ${a}) / 255`);
        this.blue(`(b * (255 - ${a}) + ${b} * ${a}) / 255`);
        return this;
    }

    /**
     * Invert colors
     */
    invert(): this {
        this.red(`255 - r`);
        this.green(`255 - g`);
        this.blue(`255 - b`);
        return this;
    }

    /**
     * Set raw shader/equation code (bypasses DSL)
     * @param shaderCode Raw GLSL or custom shader code
     */
    setRawShader(shaderCode: string): this {
        this.equations = [shaderCode];
        this.definitions = [];
        this.functions = [];
        return this;
    }

    /**
     * Add raw shader code (appends to existing equations)
     * @param shaderCode Raw shader code to append
     */
    addRawShader(shaderCode: string): this {
        this.equations.push(shaderCode);
        return this;
    }

    /**
     * Build the final equation string
     */
    build(): string {
        const parts = [];
        if (this.definitions.length > 0) {
            parts.push(this.definitions.join('\n'));
        }
        if (this.functions.length > 0) {
            parts.push(this.functions.join('\n'));
        }
        if (this.equations.length > 0) {
            parts.push(this.equations.join('\n'));
        }
        return parts.join('\n');
    }

    /**
     * Reset the builder
     */
    reset(): this {
        this.definitions = [];
        this.functions = [];
        this.equations = [];
        return this;
    }
}

/**
 * Represents a shape specification for window scissoring/clipping
 */
export interface ShapeSpec {
    type: 'rect' | 'rounded-rect' | 'circle' | 'ellipse' | 'path' | 'cutout';
    x?: number;
    y?: number;
    width?: number;
    height?: number;
    radius?: number;
    radiusX?: number;
    radiusY?: number;
    rx?: number; // top-left
    ry?: number; // top-left
    rtx?: number; // top-right
    rty?: number;
    rbx?: number; // bottom-right
    rby?: number;
    rlx?: number; // bottom-left
    rly?: number;
    cx?: number; // center x
    cy?: number; // center y
    pathString?: string; // SVG-like path string
    dx?: number; // cutout position
    dy?: number;
    cutoutRadius?: number;
}

/**
 * Builder for creating window shapes with rounding and cutouts
 * Supports shape specifications similar to CSS border-radius and SVG paths
 */
export class ShapeBuilder {
    private shapes: ShapeSpec[] = [];

    /**
     * Add a basic rectangle
     */
    addRect(x: number, y: number, width: number, height: number): this {
        this.shapes.push({ type: 'rect', x, y, width, height });
        return this;
    }

    /**
     * Add a rounded rectangle with uniform corner radius
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Width
     * @param height Height
     * @param radius Corner radius (0-50, as percentage relative to min dimension)
     */
    addRoundedRect(x: number, y: number, width: number, height: number, radius: number): this {
        this.shapes.push({ type: 'rounded-rect', x, y, width, height, radius });
        return this;
    }

    /**
     * Add a rounded rectangle with individual corner radii
     * Follows CSS convention: top-left, top-right, bottom-right, bottom-left
     */
    addRoundedRectAdvanced(
        x: number,
        y: number,
        width: number,
        height: number,
        rx: number,
        rtx?: number,
        rbx?: number,
        rlx?: number,
        ry?: number,
        rty?: number,
        rby?: number,
        rly?: number
    ): this {
        this.shapes.push({
            type: 'rounded-rect',
            x,
            y,
            width,
            height,
            rx,
            rtx: rtx ?? rx,
            rbx: rbx ?? rx,
            rlx: rlx ?? rx,
            ry: ry ?? rx,
            rty: rty ?? rtx ?? rx,
            rby: rby ?? rbx ?? rx,
            rly: rly ?? rlx ?? rx
        });
        return this;
    }

    /**
     * Add a circle
     * @param cx Center X
     * @param cy Center Y
     * @param radius Circle radius
     */
    addCircle(cx: number, cy: number, radius: number): this {
        this.shapes.push({ type: 'circle', cx, cy, radius });
        return this;
    }

    /**
     * Add an ellipse
     * @param cx Center X
     * @param cy Center Y
     * @param radiusX Horizontal radius
     * @param radiusY Vertical radius
     */
    addEllipse(cx: number, cy: number, radiusX: number, radiusY: number): this {
        this.shapes.push({ type: 'ellipse', cx, cy, radiusX, radiusY });
        return this;
    }

    /**
     * Add a path-based shape from SVG path string
     * Supports simplified path format: M (move), L (line), C (cubic), Q (quadratic), Z (close)
     */
    addPath(pathString: string): this {
        this.shapes.push({ type: 'path', pathString });
        return this;
    }

    /**
     * Add a circular cutout (hole) in the shape
     * @param dx Distance from left edge
     * @param dy Distance from top edge
     * @param cutoutRadius Radius of the circular cutout
     */
    addCircularCutout(dx: number, dy: number, cutoutRadius: number): this {
        this.shapes.push({ type: 'cutout', dx, dy, cutoutRadius });
        return this;
    }

    /**
     * Get all shape specifications
     */
    getShapes(): ShapeSpec[] {
        return this.shapes;
    }

    /**
     * Convert shape specifications to a JSON string for transmission
     */
    toJSON(): string {
        return JSON.stringify(this.shapes);
    }

    /**
     * Build a string representation suitable for shader code
     */
    build(): string {
        return this.toJSON();
    }

    /**
     * Reset the builder
     */
    reset(): this {
        this.shapes = [];
        return this;
    }

    /**
     * Parse CSS-like border-radius string
     * Format: "10px", "10px 20px", "10px 20px 30px 40px"
     * Or with per-corner: "10px / 5px" (horizontal / vertical radii)
     */
    static parseCSSSyntax(cssRadius: string): { rx: number; ry: number; rtx?: number; rty?: number; rbx?: number; rby?: number; rlx?: number; rly?: number } {
        const parts = cssRadius.split('/').map(p => p.trim());
        const hRadii = parts[0].split(' ').map(p => parseInt(p) || 0);
        const vRadii = parts[1]?.split(' ').map(p => parseInt(p) || 0) ?? hRadii;

        const radii: any = { rx: hRadii[0] || 0, ry: vRadii[0] || 0 };

        if (hRadii.length > 1) {
            radii.rtx = hRadii[1] || hRadii[0];
        }
        if (hRadii.length > 2) {
            radii.rbx = hRadii[2] || hRadii[0];
        }
        if (hRadii.length > 3) {
            radii.rlx = hRadii[3] || hRadii[0];
        }

        if (vRadii.length > 1) {
            radii.rty = vRadii[1] || vRadii[0];
        }
        if (vRadii.length > 2) {
            radii.rby = vRadii[2] || vRadii[0];
        }
        if (vRadii.length > 3) {
            radii.rly = vRadii[3] || vRadii[0];
        }

        return radii;
    }
}

/**
 * Create a new pixel effect builder
 */
export function createPixelEffect(): PixelEffectBuilder {
    return new PixelEffectBuilder();
}

/**
 * Create a new shape builder
 */
export function createShape(): ShapeBuilder {
    return new ShapeBuilder();
}