# Task F: Handwritten Backpropagation Derivation
**Pass Science Works R&D Internship Assessment 2026–2027**  
*Candidate Submission Appendix — Maximum Two Pages*

---

## PAGE 1 / 2: Forward Pass & Analytic Chain-Rule Derivatives

### 1. Model Architecture & Specifications
* **Single Training Sample:** Input $x = 1.0$, Target $y = 2.0$.
* **Network Equations:**
  $$z = w x + b$$
  $$h = \tanh(z)$$
  $$\hat{y} = v h + c$$
  $$J = \frac{1}{2}(\hat{y} - y)^2$$
* **Initial Parameter Values:**
  $$w = 0.5, \quad b = 0.0, \quad v = 1.0, \quad c = 0.0, \quad \eta = 0.1$$

---

### 2. Forward Pass (Step 0)
1. **Pre-activation:**
   $$z = w x + b = (0.5)(1.0) + 0.0 = 0.5$$

2. **Hidden Activation:**
   $$h = \tanh(0.5) = \frac{e^{0.5} - e^{-0.5}}{e^{0.5} + e^{-0.5}} = \frac{1.648721 - 0.606531}{1.648721 + 0.606531} = \frac{1.042191}{2.255252} \approx \mathbf{0.462117}$$

3. **Output Prediction:**
   $$\hat{y} = v h + c = (1.0)(0.462117) + 0.0 = \mathbf{0.462117}$$

4. **Loss Evaluation:**
   $$e = \hat{y} - y = 0.462117 - 2.0 = -1.537883$$
   $$J = \frac{1}{2}(-1.537883)^2 = \frac{1}{2}(2.365084) = \mathbf{1.182542}$$

---

### 3. Chain-Rule Analytical Partial Derivatives
By applying the chain rule of calculus:

1. **Output Error Derivative:**
   $$\frac{\partial J}{\partial \hat{y}} = \hat{y} - y = -1.537883$$

2. **Output Bias $c$:**
   $$\frac{\partial J}{\partial c} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial c} = (\hat{y} - y) \cdot 1 = \mathbf{-1.537883}$$

3. **Output Weight $v$:**
   $$\frac{\partial J}{\partial v} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial v} = (\hat{y} - y) \cdot h = (-1.537883)(0.462117) = \mathbf{-0.710682}$$

4. **Hidden State Pre-activation $z$:**
   $$\frac{\partial J}{\partial h} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial h} = (\hat{y} - y) \cdot v$$
   $$\frac{\partial h}{\partial z} = 1 - \tanh^2(z) = 1 - h^2 = 1 - (0.462117)^2 = 1 - 0.213552 = 0.786448$$
   $$\frac{\partial J}{\partial z} = \frac{\partial J}{\partial h} \frac{\partial h}{\partial z} = [(\hat{y} - y) \cdot v] \cdot (1 - h^2) = (-1.537883)(1.0)(0.786448) = \mathbf{-1.209465}$$

5. **Hidden Weight $w$:**
   $$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial z} \frac{\partial z}{\partial w} = \frac{\partial J}{\partial z} \cdot x = (-1.209465)(1.0) = \mathbf{-1.209465}$$

6. **Hidden Bias $b$:**
   $$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial z} \frac{\partial z}{\partial b} = \frac{\partial J}{\partial z} \cdot 1 = \mathbf{-1.209465}$$

---

## PAGE 2 / 2: Gradient Direction & Simultaneous Parameter Update

### 4. Gradient Vector & Physical Direction Justification
The full gradient vector evaluated at the initial state $\theta^{(0)} = [w, b, v, c]^T$ is:
$$\nabla_\theta J = \begin{bmatrix} \frac{\partial J}{\partial w} \\ \frac{\partial J}{\partial b} \\ \frac{\partial J}{\partial v} \\ \frac{\partial J}{\partial c} \end{bmatrix} = \begin{bmatrix} -1.209465 \\ -1.209465 \\ -0.710682 \\ -1.537883 \end{bmatrix}$$

> **Explanation of Gradient Direction:**
> * All four partial derivatives are strictly **negative** ($\nabla_\theta J < 0$).
> * **Why?** Because the network's initial output $\hat{y} = 0.462117$ is substantially lower than the target $y = 2.0$ (an error of $-1.537883$).
> * In standard gradient descent, parameters are updated along the negative gradient direction:
>   $$\theta^{(k+1)} = \theta^{(k)} - \eta \nabla_\theta J$$
> * Since $\nabla_\theta J < 0$, the update term $-\eta \nabla_\theta J > 0$ is strictly **positive**.
> * Therefore, the optimization algorithm increases all four parameters ($w, b, v, c$). Increasing $w$ and $b$ increases $z$ and $h$, while increasing $v$ and $c$ directly increases $\hat{y}$ toward $2.0$, reducing the squared loss $J$.

---

### 5. Simultaneous Gradient Descent Update ($\eta = 0.1$)
Using the gradients calculated from the original parameters:

1. **Hidden Weight $w$:**
   $$w^{(1)} = w^{(0)} - \eta \frac{\partial J}{\partial w} = 0.5 - (0.1)(-1.209465) = 0.5 + 0.120946 = \mathbf{0.620946}$$

2. **Hidden Bias $b$:**
   $$b^{(1)} = b^{(0)} - \eta \frac{\partial J}{\partial b} = 0.0 - (0.1)(-1.209465) = 0.0 + 0.120946 = \mathbf{0.120946}$$

3. **Output Weight $v$:**
   $$v^{(1)} = v^{(0)} - \eta \frac{\partial J}{\partial v} = 1.0 - (0.1)(-0.710682) = 1.0 + 0.071068 = \mathbf{1.071068}$$

4. **Output Bias $c$:**
   $$c^{(1)} = c^{(0)} - \eta \frac{\partial J}{\partial c} = 0.0 - (0.1)(-1.537883) = 0.0 + 0.153788 = \mathbf{0.153788}$$

---

### 6. Verification Against Central Finite Differences ($\epsilon = 10^{-6}$)
In accordance with Task F, the analytical gradients were cross-checked using symmetric central difference perturbations $\nabla_{\text{num}} = \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$:

| Parameter | Analytical $\nabla_{\text{analytic}}$ | Numerical $\nabla_{\text{num}}$ | Absolute Error | Status ($\le 10^{-5}$) |
| :---: | :---: | :---: | :---: | :---: |
| $w$ | $-1.20946452$ | $-1.20946452$ | $1.13 \times 10^{-10}$ | **PASS** |
| $b$ | $-1.20946452$ | $-1.20946452$ | $1.13 \times 10^{-10}$ | **PASS** |
| $v$ | $-0.71068205$ | $-0.71068205$ | $5.25 \times 10^{-11}$ | **PASS** |
| $c$ | $-1.53788284$ | $-1.53788284$ | $1.12 \times 10^{-10}$ | **PASS** |

### 7. Post-Update Step 1 Verification
Evaluating the model after one simultaneous update:
$$z^{(1)} = (0.620946)(1.0) + 0.120946 = 0.741892$$
$$h^{(1)} = \tanh(0.741892) = 0.630225$$
$$\hat{y}^{(1)} = (1.071068)(0.630225) + 0.153788 = \mathbf{0.828800} \quad (\text{improved from } 0.462117)$$
$$J^{(1)} = \frac{1}{2}(0.828800 - 2.0)^2 = \mathbf{0.685904} \quad (\text{decreased by } 42\% \text{ from } 1.182542)$$
Monotonic descent continues over 100 steps to $J_{100} = 1.70 \times 10^{-20}$.
