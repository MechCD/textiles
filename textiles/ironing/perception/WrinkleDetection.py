import numpy as np
import matplotlib.pyplot as plt
from skimage.filters import frangi, hessian
from skimage.filters.rank import median
from skimage.morphology import binary_erosion, binary_dilation, disk, medial_axis, skeletonize
from skimage import img_as_ubyte
import cv2

from textiles.common.graph import dfs


def detect_wrinkles_from_file(data_file_path, debug=False,
                              depth_file_suffix="-depth_image.m", image_file_suffix="-wild_image.m",
                              mask_file_suffix="-image_mask.m"):
    # Construct filenames
    # image_filename = data_file_path + depth_file_suffix
    image_filename = data_file_path + image_file_suffix
    mask_filename = data_file_path + mask_file_suffix

    # Load data from files
    image = np.loadtxt(image_filename)
    mask = np.loadtxt(mask_filename)

    return detect_wrinkles(image, mask=mask, debug=debug)


def detect_wrinkles(image, mask=None, debug=False, use_frangi=False):
    if mask is None:
        mask = np.ones_like(image)

    # Erode mask to discard border information
    inner_mask = img_as_ubyte(binary_erosion(binary_dilation(mask, disk(3)), disk(11)))
    mask = img_as_ubyte(mask)

    # Normalize (not sure if required, WiLD should be already normalized)
    minimum = np.min(image[np.nonzero(mask)])  # 0.9
    normalized_image = np.where(mask != 0, (image - minimum) / (image.max()-minimum), 0)
    # normalized_image = np.where(mask != 0, image, 0)
    # filtered_image = median(normalized_image, disk(3))

    # Step #1: Identify garment border
    _, contours, _ = cv2.findContours(inner_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    garment_contour = max(contours, key=cv2.contourArea)

    if debug:
        # Display the image and plot garment contour
        fig, ax = plt.subplots()
        im = ax.imshow(normalized_image, interpolation='nearest', cmap=plt.cm.RdGy)
        plt.colorbar(im, orientation='horizontal')

        points = [tuple(point[0]) for point in garment_contour]
        # Plot lines
        # for (start_x, start_y), (end_x, end_y) in zip(points, points[1:]+points[0:1]):
        #     plt.plot((start_x, end_x), (start_y, end_y), 'r-', linewidth=2.0, alpha=0.7)
        # Plot points
        # for x, y in points:
        #     plt.plot(x, y, 'ro', alpha=0.7)

        ax.axis('image')
        ax.set_xticks([])
        ax.set_yticks([])

        plt.figure()
        plt.hist(np.ravel(normalized_image), bins=np.arange(0, 1.01, 0.01))
        plt.show()

    # Step #2: Identify wrinkle(s)
    if use_frangi:
        wrinkles = np.where(mask == 0, 0, frangi(image))
        H = np.where(mask == 0, 0, hessian(image))

        if debug:
            fig, ax = plt.subplots(ncols=2, subplot_kw={'adjustable': 'box-forced'})
            ax[0].imshow(wrinkles, cmap=plt.cm.RdGy)
            ax[0].set_title('Frangi filter result')
            ax[1].imshow(H, cmap=plt.cm.RdGy)
            ax[1].set_title('Hybrid Hessian filter result')

            for a in ax:
                a.axis('off')

            plt.tight_layout()
            plt.show()

        # Normalize frangi filter output and threshold
        threshold_wrinkles = 160
        min_wrinkles = np.min(wrinkles[np.nonzero(wrinkles)])
        norm_wrinkles = np.where(wrinkles != 0, (wrinkles - min_wrinkles) / (wrinkles.max()-min_wrinkles), 0)
        binary_wrinkles = img_as_ubyte(np.where(img_as_ubyte(norm_wrinkles) > threshold_wrinkles, 255, 0))

    else:
        binary_wrinkles = img_as_ubyte(np.where(np.logical_and(inner_mask,
                                                               np.logical_and(normalized_image > 0.4,
                                                                              normalized_image < 0.95)),
                                                255, 0))

    if debug:
        plt.imshow(binary_wrinkles, cmap=plt.cm.gray)
        plt.show()

    # Select largest wrinkle blob
    _, wrinkle_blobs, _ = cv2.findContours(binary_wrinkles, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not wrinkle_blobs:  # No wrinkles
        return None, 0
    current_wrinkle_contour = max(wrinkle_blobs, key=cv2.contourArea)

    if debug:
        # Display the image and plot all contours found
        plt.imshow(normalized_image, interpolation='nearest', cmap=plt.cm.RdGy)
        points = [tuple(point[0]) for point in current_wrinkle_contour]
        # Plot lines
        for (start_x, start_y), (end_x, end_y) in zip(points, points[1:]+points[0:1]):
            plt.plot((start_x, end_x), (start_y, end_y), 'r-', linewidth=2.0, alpha=0.7)
        plt.show()

    # Skeletonize wrinkle contour:
    current_wrinkle = np.zeros(image.shape, np.uint8)
    cv2.drawContours(current_wrinkle, [current_wrinkle_contour], -1, 255, -1)
    wrinkle_skeleton = skeletonize(np.where(current_wrinkle == 255, 1, 0))

    if debug:
        # display results
        fig, (ax1, ax2) = plt.subplots(nrows=1, ncols=2, figsize=(8, 4.5),
                                       sharex=True, sharey=True,
                                       subplot_kw={'adjustable': 'box-forced'})

        ax1.imshow(image, cmap=plt.cm.gray)
        ax1.axis('off')
        ax1.set_title('original', fontsize=20)

        ax2.imshow(wrinkle_skeleton, cmap=plt.cm.gray)
        ax2.axis('off')
        ax2.set_title('skeleton', fontsize=20)

        fig.tight_layout()

        plt.show()

    # From skeleton to ironing path (graph)
    ################################################################################
    # Find row and column locations that are non-zero in skeleton
    (rows, cols) = np.nonzero(wrinkle_skeleton)

    # Retrieve the path as a graph (plus extreme nodes)
    path = {}
    extreme_points = []
    for src_x, src_y in zip(cols, rows):
        for dst_x, dst_y in zip(cols, rows):
            if src_x == dst_x and src_y == dst_y:
                continue
            if np.sqrt((src_x-dst_x)**2+(src_y-dst_y)**2) < 2:
                path.setdefault((src_x, src_y), []).append((dst_x, dst_y))
        if len(path.get((src_x, src_y), [])) == 1:
            extreme_points.append((src_x, src_y))
    #  print(path)

    # Step 3 (4&5): Compute start and end points
    start = None
    end = None
    distances = []
    garment_contour_points = [tuple(point[0]) for point in garment_contour]
    for x, y in extreme_points:
        dist_to_contour = min([np.sqrt((x-i)**2 + (y-j)**2) for i, j in garment_contour_points])
        distances.append(dist_to_contour)
    start = extreme_points[distances.index(max(distances))]
    end = extreme_points[distances.index(min(distances))]

    # Compute path as a list of points
    src_node = start
    path_points = dfs(path, start, end)
    #  print(path_points)

    if debug:
        # Display the image and plot endpoints
        plt.imshow(normalized_image, interpolation='nearest', cmap=plt.cm.RdGy)
        # Plot lines
        for (start_x, start_y), (end_x, end_y) in zip(path_points, path_points[1:]):
            plt.plot((start_x, end_x), (start_y, end_y), 'r-', linewidth=2.0, alpha=0.7)
        # Plot points
        plt.plot(start[0], start[1], 'bo', alpha=0.7)
        plt.plot(end[0], end[1], 'go', alpha=0.7)
        plt.show()

    # Compute global metric
    _, contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    true_garment_contour = max(contours, key=cv2.contourArea)
    # with b-Wild
    # global_metric = sum(map(cv2.contourArea, wrinkle_blobs)) / cv2.contourArea(true_garment_contour)
    # with wild
    global_metric = np.sum(np.where(mask != 0, 1-normalized_image, 0)) / cv2.contourArea(true_garment_contour)
    return path_points, global_metric



