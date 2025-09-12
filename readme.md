### 1. 什么是“跨域”问题？

浏览器出于安全考虑，实施了一种称为 **“同源策略”** 的安全措施。

- **同源**：指的是请求的URL和提供JS代码的网页URL在**协议（http/https）、域名（[domain.com](https://domain.com/)）、端口（8080等）** 三者上完全一致。
- **跨域**：只要协议、域名、端口有任何一项不同，就被认为是“跨域”的。

**举个例子：**

- 你的JS代码运行在 `http://localhost:3000` （由React/Vue开发服务器提供）
- 你的Boost.Beast服务器运行在 `http://localhost:8080`

虽然域名都是 `localhost`，但端口不同（3000 vs 8080），这就构成了**跨域**。

### 2. 浏览器为什么要阻止？

为了防止恶意网站（A网站）的脚本偷偷地向你的银行网站（B网站）发起请求（带着你的Cookies），并窃取返回的敏感数据。

因此，浏览器规定：**一个网页中的JS脚本，默认只能访问“同源”的服务器资源**。对于“跨域”请求，浏览器会实际发出请求，但如果服务器没有明确授权，浏览器会**拦截返回的响应**，不交给你的JS代码。

这就是你遇到的状况：**服务器确实发出了响应，但被浏览器中途拦截了。**

### 3. 如何解决？—— CORS

CORS是一种机制，它允许服务器**明确地告诉浏览器**：“我允许来自某个域的客户端请求我的资源”。

实现CORS的方式是：服务器需要在HTTP响应头中添加一些特定的字段。

**最重要的几个响应头是：**

- `Access-Control-Allow-Origin`: 指定允许访问资源的源（域）。
  - `Access-Control-Allow-Origin: http://localhost:3000` （允许特定源）
  - `Access-Control-Allow-Origin: *` （允许所有源，**注意：在生产环境中慎用，会带来安全风险**）
- `Access-Control-Allow-Methods`: 指定允许的HTTP方法（如 `GET, POST, PUT, DELETE`）。
- `Access-Control-Allow-Headers`: 指定允许客户端携带的额外头信息（如 `Content-Type, Authorization`）。



