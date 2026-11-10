import AppKit
import ImageIO

let arguments = CommandLine.arguments
guard arguments.count == 3,
      let input = CGImageSourceCreateWithURL(URL(fileURLWithPath: arguments[1]) as CFURL, nil),
      let source = CGImageSourceCreateImageAtIndex(input, 0, nil),
      let canvas = CGContext(data: nil, width: 1024, height: 1024,
                             bitsPerComponent: 8, bytesPerRow: 0,
                             space: CGColorSpaceCreateDeviceRGB(),
                             bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
    fatalError("Could not open the StereoMask icon artwork")
}

let bounds = CGRect(x: 0, y: 0, width: 1024, height: 1024)
let tile = bounds.insetBy(dx: 64, dy: 64)
canvas.clear(bounds)
canvas.addPath(CGPath(roundedRect: tile, cornerWidth: 200, cornerHeight: 200, transform: nil))
canvas.clip()
canvas.interpolationQuality = .high
canvas.draw(source, in: tile)

guard let image = canvas.makeImage(),
      let png = NSBitmapImageRep(cgImage: image).representation(using: .png, properties: [:]) else {
    fatalError("Could not encode the rounded StereoMask icon")
}
try png.write(to: URL(fileURLWithPath: arguments[2]))
