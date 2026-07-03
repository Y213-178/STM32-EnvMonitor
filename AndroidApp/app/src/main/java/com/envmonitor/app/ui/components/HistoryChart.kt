package com.envmonitor.app.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.envmonitor.app.data.model.HistoryPoint

@Composable
fun HistoryChart(
    points: List<HistoryPoint>,
    lineColor: Color,
    modifier: Modifier = Modifier
) {
    if (points.isEmpty()) {
        Text(
            text = "暂无数据",
            modifier = modifier.padding(16.dp),
            color = Color.Gray
        )
        return
    }

    if (points.size == 1) {
        Text(
            text = "需要更多数据才能绘制曲线",
            modifier = modifier.padding(16.dp),
            color = Color.Gray
        )
        return
    }

    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .aspectRatio(2.5f)
            .padding(start = 52.dp, end = 16.dp, top = 16.dp, bottom = 44.dp)
    ) {
        val minVal = points.minOf { it.value }
        val maxVal = points.maxOf { it.value }
        val range = if (maxVal == minVal) 1f else maxVal - minVal
        val padRange = range * 0.1f
        val drawMin = minVal - padRange
        val drawMax = maxVal + padRange
        val drawRange = drawMax - drawMin
        val padding = 8.dp.toPx()
        val availW = size.width - padding * 2
        val availH = size.height - padding * 2

        val labelPaint = android.graphics.Paint().apply {
            color = 0xFF999999.toInt()
            textSize = 9.sp.toPx()
        }

        // Grid lines + Y-axis labels
        for (i in 0..3) {
            val y = padding + availH * i / 3
            drawLine(Color(0xFFEEEEEE), Offset(padding, y), Offset(padding + availW, y), 1f)
            val label = String.format("%.1f", drawMax - drawRange * i / 3)
            drawContext.canvas.nativeCanvas.drawText(
                label,
                2.dp.toPx(),
                y + 3.dp.toPx(),
                labelPaint
            )
        }

        // Time labels at bottom (moved further down)
        val total = points.size
        val step = maxOf(1, total / 4)
        val timeY = size.height - 2.dp.toPx()
        val timeYPaint = android.graphics.Paint().apply {
            color = 0xFF999999.toInt()
            textSize = 9.sp.toPx()
        }
        for (i in points.indices step step) {
            val x = padding + availW * i / (total - 1).coerceAtLeast(1)
            drawContext.canvas.nativeCanvas.drawText(
                points[i].timestamp,
                x - 14.dp.toPx(),
                timeY,
                timeYPaint
            )
        }
        // Always show last point time
        if (total > 1) {
            val lastX = padding + availW * (total - 1) / (total - 1).coerceAtLeast(1)
            drawContext.canvas.nativeCanvas.drawText(
                points.last().timestamp,
                lastX - 14.dp.toPx(),
                timeY,
                timeYPaint
            )
        }

        // Data line
        val path = Path()
        points.forEachIndexed { idx, pt ->
            val x = padding + availW * idx / (total - 1).coerceAtLeast(1)
            val y = padding + availH - availH * ((pt.value - drawMin) / drawRange)
            if (idx == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        drawPath(
            path = path,
            color = lineColor,
            style = Stroke(
                width = 2.5.dp.toPx(),
                cap = StrokeCap.Round,
                join = StrokeJoin.Round
            )
        )
    }
}
